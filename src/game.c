#include "game.h"
#include "camera.h"
#include "embedded_levels.h"
#include "gameplay_ai.h"
#include "gameplay_climb.h"
#include "gameplay_combat.h"
#include "gameplay_interaction.h"
#include "gameplay_physics.h"
#include "gameplay_world.h"
#include "level_art.h"
#include "version.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void game_enter_state(Game *game, GameState next_state);
/* Defined with the rest of the options sheet, below, but needed by game_init:
 * the window and the audio device are both opened to match a file that has to
 * have been read first. */
static void game_load_settings(Game *game);
static void game_apply_volumes(Game *game);

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

/*
 * Turns one buffer of gameplay events into audio and presentation. Every
 * simulation in the game (the platformer and the prologue pursuit) reports
 * feedback the same way, so they share this one translation point.
 */
static void dispatch_events(Game *game, GameEventBuffer *events,
                            float listener_x, float listener_y)
{
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
        case GAME_EVENT_DUST:
            particle_system_dust(&game->presentation.particles,
                                 event->data.dust.x, event->data.dust.y,
                                 event->data.dust.count,
                                 event->data.dust.spread);
            break;
        case GAME_EVENT_CHATTER:
        {
            /* Earshot is measured from the same listener the positional audio
             * uses, so the words and the voice that carries them can never
             * disagree about whether Chuck was close enough. */
            float dx = event->data.chatter.x - listener_x;
            float dy = event->data.chatter.y - listener_y;
            if (dx * dx + dy * dy > CHATTER_EARSHOT * CHATTER_EARSHOT)
                break;
            game->presentation.chatter_kind = event->data.chatter.kind;
            game->presentation.chatter_speaker = event->data.chatter.speaker;
            game->presentation.chatter_roll = event->data.chatter.roll;
            game->presentation.chatter_timer = CHATTER_HOLD_TIME;
            break;
        }
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

static void dispatch_gameplay_events(Game *game)
{
    float player_height = game->gameplay.player.crawling
                              ? (float)PLAYER_CRAWL_H
                              : (float)PLAYER_H;
    dispatch_events(game, &game->gameplay.events,
                    game->gameplay.player.x + PLAYER_W * 0.5f,
                    game->gameplay.player.y + player_height * 0.5f);
}

static void reset_level_presentation(Game *game)
{
    game->presentation.message_timer = 0.0f;
    game->presentation.exit_unlocked_timer = 0.0f;
    game->presentation.extra_life_timer = 0.0f;
    /* Otherwise a sector opens still printing what somebody said in the one
     * before it. */
    game->presentation.chatter_timer = 0.0f;
    game->presentation.camera_shake_timer = 0.0f;
    game->presentation.camera_shake_duration = 0.0f;
    game->presentation.camera_shake_strength = 0.0f;
    game->presentation.camera_shake_x = 0.0f;
    game->presentation.camera_shake_y = 0.0f;
    game->presentation.footstep_audio_timer = 0.0f;
    game->presentation.ladder_audio_timer = 0.0f;
    game->presentation.footstep_alternate = false;
    /* Otherwise the figure would open a sector still compressed from the drop
     * that ended the last one. */
    game->presentation.player_land_squash = 0.0f;
    SDL_memset(game->presentation.fall_platform_sounded, 0,
               sizeof(game->presentation.fall_platform_sounded));
}

static void reset_sublevel_visit(Game *game)
{
    memset(&game->inactive_gameplay, 0, sizeof(game->inactive_gameplay));
    memset(game->inactive_fall_platform_sounded, 0,
           sizeof(game->inactive_fall_platform_sounded));
    game->sublevel_initialized = false;
    game->in_sublevel = false;
    game->main_level_cam_x = 0.0f;
    game->main_level_cam_y = 0.0f;
}

static void transfer_player_loadout(Player *destination,
                                    const Player *source)
{
    destination->bullets = source->bullets;
    destination->grenades = source->grenades;
    destination->bazooka_rockets = source->bazooka_rockets;
    destination->active_weapon = source->active_weapon;
    destination->facing = source->facing;
}

/* Hand the shell-owned assist choices to a simulation as plain flags. The
 * gameplay core stays deterministic and never knows a menu exists. */
static void apply_assist_to_state(Game *game, GameplayState *state)
{
    state->assist_slow_enemies = game->settings.assist.slower_guards;
    state->assist_more_hearts = game->settings.assist.more_hearts;
    if (state->player.hp > gameplay_player_max_hp(state))
        state->player.hp = gameplay_player_max_hp(state);
}

static void camera_target(Game *game, float *target_x, float *target_y)
{
    int win_w = 0, win_h = 0;
    game_get_view_size(game, &win_w, &win_h);

    const Player *player = &game->gameplay.player;
    float player_height = player->crawling ? (float)PLAYER_CRAWL_H
                                           : (float)PLAYER_H;
    float world_width =
        game->gameplay.level.map.width * (float)TILE_SIZE;
    float world_height =
        game->gameplay.level.map.height * (float)TILE_SIZE;

    *target_x = camera_axis_target(player->x + PLAYER_W * 0.5f,
                                   world_width, (float)win_w);
    *target_y = camera_axis_target(player->y + player_height * 0.5f,
                                   world_height,
                                   (float)(win_h - HUD_HEIGHT));
}

static void snap_camera_to_player(Game *game)
{
    camera_target(game, &game->presentation.cam_x,
                  &game->presentation.cam_y);
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
    apply_assist_to_state(game, restroom);
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
    /* Which cracked panels have already been heard is presentation, but it is
     * indexed by a panel of *this* level, so it has to change hands with the
     * level the way the camera does. Left behind, the room's panel n would
     * inherit the sector's and swallow its crack; cleared instead, every panel
     * already gone from the sector would crack a second time on the way back,
     * because `triggered` stays set for the rest of the run. */
    for (int i = 0; i < MAX_FALL_PLATFORMS; ++i)
    {
        bool held = game->presentation.fall_platform_sounded[i];
        game->presentation.fall_platform_sounded[i] =
            game->inactive_fall_platform_sounded[i];
        game->inactive_fall_platform_sounded[i] = held;
    }
    /* The restroom is a room of its own, not a corner of the sector, so it is
     * scored as one. Both directions of the door come through here, which is
     * why this is the only place that has to know. */
    audio_play_music(&game->platform.audio,
                     level_theme_music(game->gameplay.level.map.theme));
}

static bool enter_restroom(Game *game)
{
    if (game->in_sublevel)
        return false;
    if (!game->sublevel_initialized && !initialize_restroom(game))
        return false;

    Player travelling_player = game->gameplay.player;
    game->main_level_cam_x = game->presentation.cam_x;
    game->main_level_cam_y = game->presentation.cam_y;
    swap_gameplay_areas(game);
    transfer_player_loadout(&game->gameplay.player, &travelling_player);
    /* Hearts travel with the loadout: the door is not a heal. */
    game->gameplay.player.hp = travelling_player.hp;
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
    game->gameplay.player.hp = travelling_player.hp;
    game->gameplay.teleport_cooldown = TELEPORT_COOLDOWN;
    game_events_clear(&game->gameplay.events);
    game_events_sound(&game->gameplay.events, SFX_DOOR);
    particle_system_clear(&game->presentation.particles);
    game->in_sublevel = false;
    game->presentation.cam_x = game->main_level_cam_x;
    game->presentation.cam_y = game->main_level_cam_y;
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
    audio_play_music(&game->platform.audio,
                     level_theme_music(game->gameplay.level.map.theme));

    player_reset(&game->gameplay.player, &game->gameplay.level);
    apply_assist_to_state(game, &game->gameplay);
    game->gameplay.player.hp = gameplay_player_max_hp(&game->gameplay);

    game->campaign.level_elapsed_time = 0.0f;
    game->campaign.level_start_score = game->campaign.score;
    game->campaign.level_deaths = 0;
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
    /* Keep the existing window size; initialise camera around the spawn. */
    snap_camera_to_player(game);

    return true;
}

static void restart_game(Game *game)
{
    campaign_reset(&game->campaign);
    load_level(game, 0);
    audio_play(&game->platform.audio, SFX_MENU_START);
}

static bool continue_game(Game *game)
{
    int level = game->campaign.current_level;
    if (!campaign_accept_continue(&game->campaign))
        return false;

    audio_stop_effects(&game->platform.audio);
    if (!load_level(game, level))
    {
        SDL_Log("Could not continue from level %d", level);
        game_enter_state(game, STATE_GAME_OVER);
        return true;
    }
    audio_play(&game->platform.audio, SFX_RESPAWN);
    return true;
}

bool game_init(Game *game)
{
    return game_init_seeded(game, (uint64_t)time(NULL));
}

bool game_init_seeded(Game *game, uint64_t seed)
{
    SDL_zerop(game);
    rng_seed(&game->gameplay.rng, seed);
    /* Before the window and before the audio device, because both of them are
     * opened to match what the player last chose. */
    game_load_settings(game);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
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

    campaign_reset(&game->campaign);
    if (!load_level(game, 0))
    {
        SDL_DestroyRenderer(game->platform.renderer);
        game->platform.renderer = NULL;
        SDL_DestroyWindow(game->platform.window);
        game->platform.window = NULL;
        SDL_Quit();
        return false;
    }

    game_input_init(game);

    /* Sound is optional. If no playback device exists, gameplay stays intact. */
    audio_init(&game->platform.audio);
    /* The saved levels reach the mixer before the first note does, so a player
     * who turned the score down does not get one bar of it at full on every
     * launch. */
    game_apply_volumes(game);
    audio_play_music(&game->platform.audio, MUSIC_INTRO);

    /* The window was created windowed; if the file says otherwise this is
     * where it is put right, once the renderer's logical presentation is set
     * so the first fullscreen frame is already letterboxed correctly. */
    if (game->settings.fullscreen)
        game_set_fullscreen(game, true);

    /* Initialise particle system */
    particle_system_init(&game->presentation.particles);

    /* Boot straight to the title screen; the prologue's three beats — the
     * kerb, the drive, the tower's front door — play after START. */
    game_enter_state(game, STATE_INTRO);

    game->platform.last_tick = SDL_GetTicksNS();
    return true;
}

void game_open_manual(Game *game)
{
    /* The manual is read from the title screen, where nothing is running: no
     * simulation to pause and no music to change. */
    audio_play(&game->platform.audio, SFX_MENU_PAGE);
    game_enter_state(game, STATE_MANUAL);
}

void game_return_to_intro(Game *game)
{
    particle_system_clear(&game->presentation.particles);
    audio_stop_effects(&game->platform.audio);
    audio_play_music(&game->platform.audio, MUSIC_INTRO);
    audio_play(&game->platform.audio, SFX_MENU_BACK);
    game_enter_state(game, STATE_INTRO);
}

bool game_start_at_level(Game *game, int level_index)
{
    campaign_reset(&game->campaign);
    audio_stop_effects(&game->platform.audio);
    if (!load_level(game, level_index))
    {
        SDL_Log("Could not start at level %d", level_index + 1);
        audio_play_music(&game->platform.audio, MUSIC_INTRO);
        game_enter_state(game, STATE_INTRO);
        return false;
    }

    audio_play(&game->platform.audio, SFX_MENU_START);
    return true;
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
    game->campaign.level_deaths++;

    bool out_of_lives = false;
    if (!game->settings.assist.infinite_lives)
        out_of_lives = campaign_lose_life(&game->campaign);
    game->gameplay.invuln_timer = INVULN_TIME;

    if (out_of_lives)
    {
        audio_stop_music(&game->platform.audio);
        if (campaign_begin_continue(&game->campaign))
            game_enter_state(game, STATE_CONTINUE);
        else
            game_enter_state(game, STATE_GAME_OVER);
    }
    else
    {
        /* A death costs the walk back, never the kit: the grenade and the
         * rocket saved for the hard part survive it, and the sidearm is
         * topped back up as the consolation. */
        Player fallen = game->gameplay.player;
        player_reset(&game->gameplay.player, &game->gameplay.level);
        transfer_player_loadout(&game->gameplay.player, &fallen);
        game->gameplay.player.bullets = MAX_AMMO;
        game->gameplay.player.hp = gameplay_player_max_hp(&game->gameplay);
        /* Progress already banked is kept: the climb resumes from the last
         * banked floor, an interior from the last card, terminal or door. */
        gameplay_restore_checkpoint(&game->gameplay);
        snap_camera_to_player(game);
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
    game->input.switch_weapon = false;
    game->input.switch_weapon_back = false;
}

void game_toggle_pause(Game *game)
{
    if (game->state == STATE_PAUSED)
    {
        audio_play(&game->platform.audio, SFX_MENU_PAGE);
        /* Resume the exact state that was interrupted. Going through
         * game_enter_state would replay STATE_LEVEL_START's reveal. */
        game->state = game->pause_return_state;
        clear_edge_input(game);
        return;
    }
    if (game->state != STATE_PLAYING && game->state != STATE_LEVEL_START &&
        game->state != STATE_SHOW_KEYCARD && game->state != STATE_CHASE)
        return;

    game->pause_return_state = game->state;
    /* The cursor always opens on RESUME. A menu that remembers where it was
     * left is a menu where the next press of confirm might abandon the run,
     * and the one item on this list that cannot be undone must never be the
     * one sitting under the thumb. */
    game->pause_cursor = PAUSE_ITEM_RESUME;
    audio_play(&game->platform.audio, SFX_MENU_PAGE);
    game_enter_state(game, STATE_PAUSED);
}

void game_pause_move_cursor(Game *game, int delta)
{
    if (game->state != STATE_PAUSED || delta == 0)
        return;
    game->pause_cursor = (game->pause_cursor + delta + PAUSE_ITEM_COUNT) %
                         PAUSE_ITEM_COUNT;
    audio_play(&game->platform.audio, SFX_MENU_PAGE);
}

void game_pause_activate(Game *game)
{
    if (game->state != STATE_PAUSED)
        return;

    switch (game->pause_cursor)
    {
    case PAUSE_ITEM_SETTINGS:
        game_open_settings(game);
        return;
    case PAUSE_ITEM_ABANDON:
        game_return_to_intro(game);
        return;
    case PAUSE_ITEM_RESUME:
    case PAUSE_ITEM_COUNT:
        break;
    }
    game_toggle_pause(game);
}

/* ---- The options sheet ---------------------------------------------- */

/* Assist changes take effect immediately in whatever is running, so a switch
 * flipped from the pause screen is felt on the very next frame. */
static void game_apply_assist_everywhere(Game *game)
{
    apply_assist_to_state(game, &game->gameplay);
    if (game->sublevel_initialized)
        apply_assist_to_state(game, &game->inactive_gameplay);
}

static void game_apply_volumes(Game *game)
{
    audio_set_volumes(&game->platform.audio,
                      (float)game->settings.music_volume / 100.0f,
                      (float)game->settings.sfx_volume / 100.0f);
}

void game_set_fullscreen(Game *game, bool on)
{
    if (game->platform.window != NULL &&
        !SDL_SetWindowFullscreen(game->platform.window, on))
    {
        /* The window refused, so the setting must not claim otherwise: a sheet
         * reading FULLSCREEN ON over a windowed game is worse than the failure
         * it is reporting. */
        SDL_Log("Could not toggle fullscreen: %s", SDL_GetError());
        game->settings.fullscreen = game->platform.fullscreen;
        return;
    }
    game->platform.fullscreen = on;
    game->settings.fullscreen = on;
}

/*
 * Where the settings file lives. SDL answers this per platform — Application
 * Support on macOS, AppData on Windows, XDG on Linux — and it is the only part
 * of the settings that needs a platform at all, which is why it is here and not
 * in [settings.c](settings.c).
 */
static bool settings_file_path(char *out, size_t cap)
{
    char *dir = SDL_GetPrefPath(CHUCK_APP_ORG, CHUCK_APP_NAME);
    if (dir == NULL)
        return false;
    int written = SDL_snprintf(out, cap, "%ssettings.cfg", dir);
    SDL_free(dir);
    return written > 0 && (size_t)written < cap;
}

static void game_load_settings(Game *game)
{
    settings_defaults(&game->settings);

    char path[1024];
    if (!settings_file_path(path, sizeof(path)))
        return;

    size_t size = 0;
    void *data = SDL_LoadFile(path, &size);
    if (data == NULL)
        return; /* No file yet: the defaults are the answer, not an error. */

    /* SDL_LoadFile terminates what it returns, so the text is already a
     * string; the size is only worth checking for the empty file. */
    if (size > 0)
        settings_parse(&game->settings, (const char *)data);
    SDL_free(data);
}

void game_save_settings(const Game *game)
{
    char path[1024];
    if (!settings_file_path(path, sizeof(path)))
        return;

    char text[512];
    size_t len = settings_serialize(&game->settings, text, sizeof(text));
    if (len == 0)
        return;
    if (!SDL_SaveFile(path, text, len))
        SDL_Log("Could not save settings: %s", SDL_GetError());
}

void game_open_settings(Game *game)
{
    if (game->state != STATE_INTRO && game->state != STATE_PAUSED)
        return;
    game->settings_return_state = game->state;
    game->settings_cursor = settings_first_row();
    audio_play(&game->platform.audio, SFX_MENU_PAGE);
    game_enter_state(game, STATE_SETTINGS);
}

void game_close_settings(Game *game)
{
    if (game->state != STATE_SETTINGS)
        return;
    audio_play(&game->platform.audio, SFX_MENU_BACK);
    game_save_settings(game);
    if (game->settings_return_state == STATE_PAUSED)
        game->state = STATE_PAUSED;
    else
        game_enter_state(game, STATE_INTRO);
}

void game_settings_move_cursor(Game *game, int delta)
{
    if (game->state != STATE_SETTINGS || delta == 0)
        return;
    game->settings_cursor = settings_move_cursor(game->settings_cursor, delta);
    audio_play(&game->platform.audio, SFX_MENU_PAGE);
}

void game_settings_adjust(Game *game, int delta)
{
    if (game->state != STATE_SETTINGS)
        return;

    int row_count = 0;
    const SettingRow *rows = settings_rows(&row_count);
    if (game->settings_cursor < 0 || game->settings_cursor >= row_count)
        return;

    SettingId id = rows[game->settings_cursor].id;
    if (!settings_adjust(&game->settings, id, delta))
        return; /* A slider already at either end: nothing moved, nothing clicks. */

    /*
     * A setting is not a stored preference until something has actually done
     * what it says. Each one reaches its own system here, in the one place that
     * knows a value has just changed.
     */
    switch (id)
    {
    case SETTING_MUSIC_VOLUME:
    case SETTING_SFX_VOLUME:
        game_apply_volumes(game);
        break;
    case SETTING_FULLSCREEN:
        game_set_fullscreen(game, game->settings.fullscreen);
        break;
    case SETTING_MORE_HEARTS:
        game_apply_assist_everywhere(game);
        /* Turning the bigger pool on fills it: an assist that arrives as two
         * empty sockets would read as a penalty. */
        if (game->settings.assist.more_hearts && !game->gameplay.player.dying)
            game->gameplay.player.hp = gameplay_player_max_hp(&game->gameplay);
        break;
    case SETTING_SLOWER_GUARDS:
        game_apply_assist_everywhere(game);
        break;
    case SETTING_CRT_FILTER:
    case SETTING_INFINITE_LIVES:
    case SETTING_NONE:
        /* Read where they are used: the finishing pass at the bottom of
         * game_render, and the death that would have cost a life. */
        break;
    }

    /* A level slides and a switch clicks: the two rows do different things and
     * must not answer with the same noise. */
    audio_play(&game->platform.audio,
               rows[game->settings_cursor].kind == SETTING_ROW_SLIDER
                   ? SFX_MENU_PAGE
                   : SFX_CARD_TARGET);
}

static void game_enter_state(Game *game, GameState next_state)
{
    switch (next_state)
    {
    case STATE_ABDUCTION:
        abduction_cutscene_init(&game->presentation.abduction_cutscene);
        reset_level_presentation(game);
        audio_stop_effects(&game->platform.audio);
        audio_play_music(&game->platform.audio, MUSIC_INTRO);
        break;
    case STATE_CHASE:
    {
        /* The pursuit draws from the campaign RNG, so one game seed still
         * decides the whole run: the drive and every level after it. */
        uint64_t seed = ((uint64_t)rng_next(&game->gameplay.rng) << 32) ^
                        (uint64_t)rng_next(&game->gameplay.rng);
        chase_init(&game->chase, seed);
        reset_level_presentation(game);
        audio_stop_effects(&game->platform.audio);
        audio_play_music(&game->platform.audio, MUSIC_PURSUIT);
        break;
    }
    case STATE_OPENING_CUTSCENE:
        opening_cutscene_init(&game->presentation.opening_cutscene);
        /* Arriving from the drive: drop the engine and the road music before
         * the rain-soaked street outside the building. */
        audio_stop_effects(&game->platform.audio);
        audio_play_music(&game->platform.audio, MUSIC_INTRO);
        break;
    case STATE_INTRO:
    {
        int win_w = 0;
        int win_h = 0;
        game_get_view_size(game, &win_w, &win_h);
        intro_init(&game->presentation.intro, win_w, win_h);
        break;
    }
    case STATE_MANUAL:
    {
        int win_w = 0;
        int win_h = 0;
        game_get_view_size(game, &win_w, &win_h);
        manual_init(&game->presentation.manual, win_w, win_h,
                    game_pad_hints(game));
        break;
    }
    case STATE_LEVEL_START:
        level_reveal_init(&game->gameplay.level);
        break;
    case STATE_OUTRO:
        outro_cutscene_init(&game->presentation.outro_cutscene);
        break;
    case STATE_CREDITS:
    {
        int win_w = 0;
        int win_h = 0;
        game_get_view_size(game, &win_w, &win_h);
        credits_init(&game->presentation.credits, (float)win_h);
        /* The wreck is still burning under the last card; the roll is not the
         * place for it. The title theme is already playing and stays playing,
         * straight through the roll and into the title screen it ends on. */
        audio_stop_effects(&game->platform.audio);
        audio_play_music(&game->platform.audio, MUSIC_INTRO);
        break;
    }
    case STATE_LEVEL_CLEARED:
        game->presentation.message_timer = 1.2f;
        break;
    case STATE_GAME_OVER:
        game->presentation.message_timer = GAME_OVER_DISPLAY_TIME;
        audio_play(&game->platform.audio, SFX_GAME_OVER);
        break;
    case STATE_SHOW_KEYCARD:
    case STATE_PLAYING:
    case STATE_PAUSED:
    case STATE_SETTINGS:
    case STATE_LEVEL_TRANSITION:
    case STATE_CONTINUE:
        break;
    }
    game->state = next_state;
}

static bool update_scene(Game *game, float dt)
{
    if (game->state == STATE_ABDUCTION)
    {
        Uint32 cues = 0;
        bool finished = abduction_cutscene_update(
            &game->presentation.abduction_cutscene, dt, &cues);

        if (cues & ABDUCTION_CUE_RAIN)
            audio_play(&game->platform.audio, SFX_OPENING_RAIN);
        if (cues & ABDUCTION_CUE_SUV_ROLL)
            audio_play(&game->platform.audio, SFX_OPENING_SUV_ENGINE);
        if (cues & ABDUCTION_CUE_SUV_BRAKE)
            audio_play(&game->platform.audio, SFX_OPENING_BRAKE);
        if (cues & ABDUCTION_CUE_CAR_DOOR)
            audio_play(&game->platform.audio, SFX_OPENING_CAR_DOOR);
        if (cues & ABDUCTION_CUE_SCREAM)
            audio_play(&game->platform.audio, SFX_CIVILIAN_SCREAM);
        if (cues & ABDUCTION_CUE_STEP_A)
            audio_play(&game->platform.audio, SFX_STEP_A);
        if (cues & ABDUCTION_CUE_STEP_B)
            audio_play(&game->platform.audio, SFX_STEP_B);
        if (cues & ABDUCTION_CUE_SUV_AWAY)
            audio_play(&game->platform.audio, SFX_OPENING_CAR_ENGINE);

        if (finished || game->input.confirm)
        {
            audio_stop_effects(&game->platform.audio);
            game_enter_state(game, STATE_CHASE);
        }
        clear_edge_input(game);
        return true;
    }

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
            /* START pressed: the prologue plays its three beats before the
             * campaign — the kerb, the drive, and the tower's front door. */
            game->input.confirm = false;
            game_enter_state(game, STATE_ABDUCTION);
        }
        clear_edge_input(game);
        return true;
    }

    if (game->state == STATE_MANUAL)
    {
        float win_x = 0.0f, win_y = 0.0f, mx = 0.0f, my = 0.0f;
        SDL_GetMouseState(&win_x, &win_y);
        SDL_RenderCoordinatesFromWindow(game->platform.renderer, win_x, win_y,
                                        &mx, &my);

        int win_w = 0, win_h = 0;
        game_get_view_size(game, &win_w, &win_h);
        manual_update(&game->presentation.manual, dt, win_w, win_h, mx, my,
                      game_pad_hints(game));
        clear_edge_input(game);
        return true;
    }

    if (game->state == STATE_SETTINGS)
    {
        /* All interaction happens in the input layer; the sheet just holds. */
        clear_edge_input(game);
        return true;
    }

    if (game->state == STATE_PAUSED)
    {
        /* Time stands still; confirm answers whichever item the cursor is on,
         * and everything else is handled by the input layer (ESC or B resumes,
         * SELECT abandons the run). */
        if (game->input.confirm)
            game_pause_activate(game);
        clear_edge_input(game);
        return true;
    }

    if (game->state == STATE_CHASE)
    {
        game_events_clear(&game->chase.events);
        ChaseOutcome outcome = chase_update(&game->chase, &game->input, dt);
        dispatch_events(game, &game->chase.events,
                        game->chase.player.x, game->chase.player.y);
        update_camera_shake(game, dt);
        if (outcome == CHASE_REACHED_BUILDING)
            game_enter_state(game, STATE_OPENING_CUTSCENE);
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

        /* The thank-you frame is held for the rest of the outro's own clock and
         * then hands over to the roll of names, which is what carries the
         * player back to the title screen. Replaying is still a press of R on
         * that card; from here on it is START on the title screen. */
        if (game->presentation.outro_cutscene.time >= OUTRO_CUTSCENE_DURATION)
            game_enter_state(game, STATE_CREDITS);

        clear_edge_input(game);
        return true;
    }

    if (game->state == STATE_CREDITS)
    {
        /* Confirm means "get on with it" while the names are moving and "done"
         * once they have stopped, which is the same rule every other card in
         * the game follows. Nobody has to press anything at all: the roll runs
         * out on its own and ESC leaves early, the way it does from a cutscene.
         * The pad's B is inert here for that same reason — there is nothing
         * open to close and no run left to drop. */
        if (game->input.confirm)
        {
            if (credits_at_rest(&game->presentation.credits))
            {
                game_return_to_intro(game);
                clear_edge_input(game);
                return true;
            }
            credits_skip_to_rest(&game->presentation.credits);
            audio_play(&game->platform.audio, SFX_MENU_PAGE);
        }

        if (credits_update(&game->presentation.credits, dt))
            game_return_to_intro(game);

        clear_edge_input(game);
        return true;
    }

    if (game->state == STATE_CONTINUE)
    {
        if (game->input.confirm && continue_game(game))
        {
            clear_edge_input(game);
            return true;
        }

        if (campaign_update_continue(&game->campaign, dt))
            game_enter_state(game, STATE_GAME_OVER);

        clear_edge_input(game);
        return true;
    }

    if (game->state == STATE_GAME_OVER)
    {
        game->presentation.message_timer -= dt;
        if (game->input.confirm || game->presentation.message_timer <= 0.0f)
            game_return_to_intro(game);
        clear_edge_input(game);
        return true;
    }

    /* Level start reveal animation: show tiles progressively, then spawn entities. */
    if (game->state == STATE_LEVEL_START)
    {
        bool skip_reveal = game->input.confirm;
        /* Do not defer actions pressed during a non-interactive transition. */
        clear_edge_input(game);

        if (skip_reveal && !game->gameplay.level.reveal.done)
        {
            /* Confirm finishes the reveal in one step. */
            level_reveal_step(&game->gameplay.level, 1000.0f);
            audio_play(&game->platform.audio, SFX_REVEAL_TICK);
        }

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
        if (!game->gameplay.level.map.has_window &&
            game->gameplay.level.runtime.card_count > 0 &&
            game->gameplay.level.runtime.active_card_index >= 0)
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
            if (game->gameplay.level.map.mode == LEVEL_MODE_INTERIOR)
                gameplay_ai_spawn_level_entities(&game->gameplay);
            game_enter_state(game, STATE_PLAYING);
        }
    }

    /* Key-card intro animation: cycle highlight until target reached, then begin play */
    if (game->state == STATE_SHOW_KEYCARD)
    {
        if (game->input.confirm &&
            game->presentation.card_anim_step <
                game->presentation.card_anim_total_steps)
        {
            /* Confirm lands the sweep on its target at once. */
            int remaining = game->presentation.card_anim_total_steps -
                            game->presentation.card_anim_step;
            if (game->presentation.card_anim_count > 0)
                game->presentation.card_anim_current =
                    (game->presentation.card_anim_current + remaining) %
                    game->presentation.card_anim_count;
            game->presentation.card_anim_step =
                game->presentation.card_anim_total_steps;
            audio_play(&game->platform.audio, SFX_CARD_TARGET);
            gameplay_ai_spawn_level_entities(&game->gameplay);
            game_enter_state(game, STATE_PLAYING);
            clear_edge_input(game);
            return true;
        }
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

static bool try_finish_current_level(Game *game)
{
    if (!gameplay_player_reached_exit(&game->gameplay))
        return false;

    if ((size_t)(game->campaign.current_level + 1) < EMBEDDED_LEVEL_COUNT)
    {
        int next_level = game->campaign.current_level + 1;
        if (game->gameplay.level.map.has_window)
        {
            /* A window is a continuous physical route between inside and the
             * facade. The hostage/elevator report belongs only between normal
             * interior sectors and would contradict what is on screen here. */
            audio_stop_music(&game->platform.audio);
            if (!load_level(game, next_level))
            {
                SDL_Log("Could not follow window route to level %d",
                        next_level);
                return false;
            }
        }
        else
        {
            level_transition_init(
                &game->presentation.level_transition,
                game->campaign.current_level,
                next_level,
                game->campaign.level_elapsed_time,
                game->campaign.score - game->campaign.level_start_score,
                gameplay_neutralized_hostiles(&game->gameplay),
                game->campaign.level_deaths);
            audio_stop_music(&game->platform.audio);
            game_enter_state(game, STATE_LEVEL_TRANSITION);
        }
    }
    else
    {
        game_enter_state(game, STATE_LEVEL_CLEARED);
    }
    game_events_sound(&game->gameplay.events, SFX_LEVEL_CLEAR);
    return true;
}

static void update_follow_camera(Game *game, float dt)
{
    float desired_x = 0.0f;
    float desired_y = 0.0f;
    camera_target(game, &desired_x, &desired_y);
    float alpha = 8.0f * dt;
    if (alpha > 1.0f)
        alpha = 1.0f;
    game->presentation.cam_x +=
        (desired_x - game->presentation.cam_x) * alpha;
    game->presentation.cam_y +=
        (desired_y - game->presentation.cam_y) * alpha;
}

static void update_facade_playing(Game *game, float dt)
{
    game->campaign.level_elapsed_time += dt;
    if (game->gameplay.invuln_timer > 0.0f)
        game->gameplay.invuln_timer -= dt;

    gameplay_climb_update_player(&game->gameplay, &game->input, dt);
    game->input.jump = false;
    game->input.shoot = false;
    game->input.use_door = false;
    game->input.switch_weapon = false;
    game->input.switch_weapon_back = false;
    gameplay_climb_update(&game->gameplay, dt);
    /* Loadout carried up the wall is spent inside the next sector, so the
     * detour to a pickup is the climb's own risk/reward decision. */
    gameplay_collect_items(&game->gameplay, &game->campaign, dt);

    while (campaign_check_extra_life(&game->campaign))
    {
        game_events_sound(&game->gameplay.events, SFX_PICKUP_HEALTH);
        game->presentation.extra_life_timer = 2.5f;
    }
    if (game->presentation.extra_life_timer > 0.0f)
        game->presentation.extra_life_timer -= dt;

    if (!try_finish_current_level(game))
        update_follow_camera(game, dt);
}

static void update_playing(Game *game, float dt)
{
    /* Above the facade branch, because the men leaning out of the windows are
     * on the same net as the ones inside and their line has to expire too. */
    if (game->presentation.chatter_timer > 0.0f)
    {
        game->presentation.chatter_timer -= dt;
        if (game->presentation.chatter_timer < 0.0f)
            game->presentation.chatter_timer = 0.0f;
    }

    if (game->gameplay.level.map.mode == LEVEL_MODE_FACADE)
    {
        update_facade_playing(game, dt);
        return;
    }

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
    int previous_elevator = game->gameplay.player_on_elevator;
    int previous_moving_platform =
        game->gameplay.player_on_moving_platform;

    /* --- Elevator: pre-carry (upward), player physics, update platforms, snap --- */
    gameplay_carry_player_on_elevator(&game->gameplay, dt);
    gameplay_resolve_player_crush(&game->gameplay);

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
        player_input.switch_weapon = false;
        player_input.switch_weapon_back = false;
        game->input.shoot = false;
        game->input.use_door = false;
        game->input.switch_weapon = false;
        game->input.switch_weapon_back = false;
        game->gameplay.player.vx = 0.0f;
    }
    float player_fall_speed =
        player_update(&game->gameplay.player, &game->gameplay.level,
                      &player_input, dt);
    game->input.jump = false;

    if (gameplay_advance_terminal(&game->gameplay, &game->campaign, dt))
        game->presentation.exit_unlocked_timer = 2.5f;

    level_update_elevators(&game->gameplay.level, dt);
    level_update_falling_platforms(&game->gameplay.level, dt);
    level_update_moving_platforms(&game->gameplay.level, dt);
    gameplay_update_crates(&game->gameplay, &game->campaign, dt);
    gameplay_resolve_player_crates(&game->gameplay, prev_player_x, prev_player_y, prev_player_h);
    gameplay_ride_platforms(&game->gameplay, dt);

    /* The player module reports the frame a jump actually started — which,
     * with the jump buffer, may be frames after the press. */
    if (game->gameplay.player.jumped)
        game_events_sound(&game->gameplay.events, SFX_JUMP);
    gameplay_handle_player_landing(&game->gameplay, player_was_grounded,
                                   player_fall_speed);

    /* The landing, as the player sees it rather than hears it: the figure
     * compresses for a beat and kicks up whatever the floor had on it. Both are
     * scaled by the drop, so stepping off a crate does not throw up the same
     * cloud as coming down a storey. */
    game->presentation.player_land_squash =
        fmaxf(0.0f, game->presentation.player_land_squash - dt * 7.0f);
    if (!player_was_grounded && game->gameplay.player.on_ground &&
        player_fall_speed > PLAYER_LAND_SOUND_SPEED)
    {
        float force = fminf(1.0f, player_fall_speed /
                                      (PLAYER_FATAL_FALL_SPEED * 0.9f));
        game->presentation.player_land_squash = 0.45f + force * 0.55f;
        particle_system_dust(&game->presentation.particles,
                             game->gameplay.player.x + PLAYER_W * 0.5f,
                             game->gameplay.player.y + PLAYER_H - 1.0f,
                             2 + (int)(force * 5.0f), 26.0f);
    }
    if (game->gameplay.player_on_elevator >= 0 && previous_elevator < 0)
        game_events_sound(&game->gameplay.events, SFX_ELEVATOR);
    if (game->gameplay.player_on_moving_platform >= 0 &&
        previous_moving_platform < 0)
    {
        const MovingPlatform *platform =
            &game->gameplay.level.runtime.moving_platforms[game->gameplay.player_on_moving_platform];
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
            /* A step is a sound and a scuff. One particle per footfall is
             * enough to say the floor has something on it, and it lands on the
             * same beat the step is heard on rather than on a timer of its
             * own. */
            if (!game->gameplay.player.crawling)
                particle_system_dust(&game->presentation.particles,
                                     game->gameplay.player.x + PLAYER_W * 0.5f,
                                     game->gameplay.player.y + PLAYER_H - 1.0f,
                                     2, 14.0f);
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
        /* The door consumes the rest of the frame, and the presses this frame
         * belong to the room being left. Left standing, an attack pressed on
         * the same frame as the door is answered on the other side of it —
         * a round spent walking through a doorway. */
        game->input.shoot = false;
        game->input.switch_weapon = false;
        game->input.switch_weapon_back = false;
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
    gameplay_update_ammo_drops(&game->gameplay, dt);
    if (!exit_was_unlocked && game->gameplay.level.runtime.exit_unlocked)
        game->presentation.exit_unlocked_timer = 2.5f;

    gameplay_combat_update_hazards(&game->gameplay);

    gameplay_combat_update_player_bullets(&game->gameplay, &game->campaign, dt);

    gameplay_ai_update_combat(&game->gameplay, dt);

    gameplay_combat_update_enemy_bullets(&game->gameplay, dt);

    gameplay_combat_check_contacts(&game->gameplay, &game->campaign);

    /* Tick the calm countdown after perception updates, so a guard or dog
     * seeing Chuck on the would-be final frame keeps the alarm alive. */
    gameplay_update_alarm(&game->gameplay, dt);

    while (campaign_check_extra_life(&game->campaign))
    {
        game_events_sound(&game->gameplay.events, SFX_PICKUP_HEALTH);
        game->presentation.extra_life_timer = 2.5f;
    }
    if (game->presentation.extra_life_timer > 0.0f)
        game->presentation.extra_life_timer -= dt;

    if (!try_finish_current_level(game))
    {
        /* Camera: follow the player and expose vertical space on tall maps. */
        update_follow_camera(game, dt);
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
    dispatch_gameplay_events(game);
    if (!scene_handled_frame)
    {
        particle_system_update(&game->presentation.particles, dt);
        update_camera_shake(game, dt);
    }
}

/* Shutdown and free game resources. */

void game_shutdown(Game *game)
{
    /* The sheet already saved on the way out; this is what catches a
     * fullscreen toggled with F and never gone back to the sheet. */
    game_save_settings(game);
    game_input_shutdown(game);
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
