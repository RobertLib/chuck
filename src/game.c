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
/* And the same for what outlives the process: load_level banks the sector it
 * opens, and every way a run can end banks the score it ended on. There are
 * five of them and they are counted in one place, at `game_record_run_score`
 * below — a number written twice is a number that drifts, and this one has. */
static void game_load_progress(Game *game);
static void game_save_progress(const Game *game);
static void game_record_run_score(Game *game);

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

    /* Asked to hold still, the shake is spent rather than skipped: the timer
     * above has already run down, so a blast still ends its shake at the
     * moment it always did and nothing downstream can tell the difference
     * except the camera. Returning early instead would leave the timer
     * standing and the next shake would find one already in progress. */
    if (game->settings.reduced_motion)
    {
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

/*
 * The room the sector's own `U` opens on — see `level_theme_sublevel`, which
 * picks it by theme the way `level_theme_music` picks the score.
 *
 * A miss cannot happen in a build the suite has passed, because
 * `test_every_restroom_theme_names_a_room_that_exists` walks the same table
 * through the same matcher against the same embedded set; falling back to the
 * first room rather than refusing the door is still the right answer for a
 * build it has not, since a `U` that swallows the press is a door the player
 * reads as broken.
 */
static const EmbeddedLevelData *restroom_source(const Game *game)
{
    const char *stem = level_theme_sublevel(game->gameplay.level.map.theme);
    for (size_t i = 0; i < EMBEDDED_SUBLEVEL_COUNT; ++i)
        if (level_sublevel_name_is(EMBEDDED_SUBLEVELS[i].name, stem))
            return &EMBEDDED_SUBLEVELS[i];

    SDL_Log("No embedded sublevel is named '%s'; opening the first one", stem);
    return &EMBEDDED_SUBLEVELS[0];
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

    const EmbeddedLevelData *source = restroom_source(game);
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

/*
 * The sector and the room change places, and the one that is not being played
 * is not ticked either — `update_playing` only ever advances `gameplay`.
 *
 * That is the decision, not an oversight, and the alarm is what settles it. A
 * countdown that kept running behind the door would make the restroom the one
 * place in the building where an alert can be waited out, which is exactly the
 * safe room the sector never granted; frozen, a detour neither winds the alarm
 * down nor stands it up, and the player comes back to the floor they left. The
 * strip says so throughout, because every field on it that names the
 * building's state reads through the *sector* rather than through whatever is
 * being simulated (see `render_hud`).
 */
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
    /*
     * A guard put down in the restroom was put down on this floor.
     *
     * The tally is a field of whichever simulation was running when he went
     * down, and inside the room that is the room's own — so the report between
     * sectors, which reads the sector's, credited the floor with one hostile
     * fewer than the player had actually cleared. The score never had the
     * problem, because `CampaignState` is shared across the door, and a room
     * that pays for a kill in points but not in the count is the strip
     * reporting the room instead of the sector, which is the one thing
     * `render_hud`'s `sector` pointer exists to prevent.
     *
     * Carried here rather than added at the kill, because the gameplay core
     * has no idea a sublevel exists and must not gain one. Zeroed on the way
     * out for the same reason the flags are swapped above: the door can be
     * used again, and a second visit must not bank the first one's kills a
     * second time.
     */
    game->gameplay.hostiles_neutralized +=
        game->inactive_gameplay.hostiles_neutralized;
    game->inactive_gameplay.hostiles_neutralized = 0;
    return true;
}

/*
 * How a sector is being entered, which decides two things that have nothing to
 * do with the map.
 *
 * The first is whether reaching it is worth writing down. The record is meant
 * to be a record of a run: the sector is banked on arrival rather than on
 * finishing, because a player who died on sector nine got to sector nine. An
 * authoring shortcut is not getting there — `--level N`, the editor's playtest
 * button and the debug picker all hand `game_start_at_level` an arbitrary
 * sector, and banking that unlocked the title screen's resume chip at a floor
 * nobody had played to.
 *
 * The second is whether the explosive in Chuck's hands survives the doorway.
 * See `load_level` for why only one of these three entries hands anything over.
 */
typedef enum
{
    /* An author put him here. Banks nothing, hands over nothing. */
    LEVEL_ENTRY_AUTHORED = 0,
    /* The first sector of a run, or the retry after a continue: a campaign
     * arrival worth banking, but not a step out of the sector below, so there
     * is nothing to bring through. */
    LEVEL_ENTRY_RUN_START,
    /* One sector to the next, by the stair door or through the window. */
    LEVEL_ENTRY_CAMPAIGN_STEP
} LevelEntry;

static bool load_level(Game *game, int index, LevelEntry entry)
{
    /* Who walked out of the sector below, copied before
     * `gameplay_state_begin_level` memsets the player along with the rest of
     * the simulation. What of him survives the doorway is
     * `player_begin_sector`'s decision and is written down there, in the
     * SDL-free half where the suite can hold it. */
    Player departed = game->gameplay.player;
    const Player *previous =
        entry == LEVEL_ENTRY_CAMPAIGN_STEP ? &departed : NULL;

    if (index < 0 || (size_t)index >= EMBEDDED_LEVEL_COUNT)
    {
        /* Said as a sector number, because that is the only way a sector is
         * ever named anywhere a human reads one — `--level N`, the strip's
         * SECTOR field, the debug picker and the editor's playtest button are
         * all 1-based. This line used to print the 0-based array index beside
         * `game_start_at_level`'s 1-based one, so a mistyped `--level 99` was
         * answered by two numbers, neither of them together and one of them
         * never typed. */
        SDL_Log("Sector %d is outside the campaign (1-%zu)", index + 1,
                EMBEDDED_LEVEL_COUNT);
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
    /* Reaching a sector is the thing worth remembering across a session, and
     * it is banked on arrival rather than on finishing: a player who dies on
     * sector nine got to sector nine, and making them earn it twice is exactly
     * the walk this is here to give back. See `LevelEntry` above for the one
     * kind of arrival that does not count. */
    if (entry != LEVEL_ENTRY_AUTHORED &&
        progress_note_sector(&game->progress, index))
        game_save_progress(game);
    audio_play_music(&game->platform.audio,
                     level_theme_music(game->gameplay.level.map.theme));

    player_begin_sector(&game->gameplay.player, &game->gameplay.level,
                        previous);
    apply_assist_to_state(game, &game->gameplay);
    game->gameplay.player.hp = gameplay_player_max_hp(&game->gameplay);
    /* A demo run gets its kit back at every doorway and starts its script over,
     * so a sector reached by playing out of the one below it is driven exactly
     * as the one `--level` opened on. Without this the hand would arrive on
     * floor two with a spent tube and cover none of the rocket art from there
     * on — which is the same silent under-coverage the switch exists to end. */
    if (game->demo_active)
    {
        demo_grant_loadout(&game->gameplay);
        demo_hand_init(&game->demo_hand);
    }

    campaign_begin_sector(&game->campaign);
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
    load_level(game, 0, LEVEL_ENTRY_RUN_START);
    audio_play(&game->platform.audio, SFX_MENU_START);
}

static bool continue_game(Game *game)
{
    int level = game->campaign.current_level;
    /* Banked before the retry is taken, because past the last continue the
     * retry starts the score again from nothing — which ends this run's
     * scoring as surely as the game-over card does. Left out, letting the
     * countdown expire kept the record and accepting the retry the prompt is
     * offering destroyed it, silently and in the player's favour nowhere. */
    game_record_run_score(game);
    if (!campaign_accept_continue(&game->campaign))
        return false;

    audio_stop_effects(&game->platform.audio);
    if (!load_level(game, level, LEVEL_ENTRY_RUN_START))
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
    /* And before the first title screen, which offers the resume it finds. */
    game_load_progress(game);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    /*
     * Resizable, and the whole cost of it is this flag: the logical
     * presentation below already fixes the coordinate system every renderer
     * lays out against, and the mouse already arrives through
     * `SDL_RenderCoordinatesFromWindow`. The window opened at a fixed 800x552
     * for a long time, which on a desktop that is now routinely four times
     * that meant a small rectangle with fullscreen as the only alternative —
     * a scaling decision taken away from the player by omission rather than
     * on purpose.
     */
    if (!SDL_CreateWindowAndRenderer("Chuck", 800, 552, SDL_WINDOW_RESIZABLE,
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

    /*
     * Vsync if the renderer will give it, and a frame floor of our own if it
     * will not.
     *
     * The swap is the only thing pacing this loop — there is no sleep anywhere
     * else in it — so a refusal here used to mean the game quietly ran at
     * whatever rate the machine could draw, pinning a core to redraw a picture
     * nobody could see change. The floor is the display's own rate where SDL
     * knows it, because that is the fastest number worth drawing at, and
     * 60 where it does not.
     */
    game->platform.vsync = SDL_SetRenderVSync(game->platform.renderer, 1);
    if (game->platform.vsync)
    {
        game->platform.frame_min_ns = 0;
    }
    else
    {
        float hz = 60.0f;
        const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode(
            SDL_GetDisplayForWindow(game->platform.window));
        if (mode != NULL && mode->refresh_rate > 1.0f)
            hz = mode->refresh_rate;
        /* Clamped at both ends: below the floor the game is slower than it was
         * tuned at, and above the ceiling this is burning a core again for
         * frames nobody asked for. */
        if (hz < 60.0f)
            hz = 60.0f;
        if (hz > 240.0f)
            hz = 240.0f;
        game->platform.frame_min_ns = (Uint64)(1.0e9f / hz);
        SDL_Log("VSync unavailable (%s); limiting to %.0f frames per second",
                SDL_GetError(), (double)hz);
    }

    SDL_srand(SDL_GetTicksNS());

    campaign_reset(&game->campaign);
    if (!load_level(game, 0, LEVEL_ENTRY_RUN_START))
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

/*
 * `--page N`, and why the book needs one when no other screen does.
 *
 * Every other thing `--scene` opens is either a still or a clock: it draws what
 * it draws, or it runs its own beats out on a timer and the smoke run only has
 * to wait. The manual is neither. It is eight sheets, each with an illustration
 * of its own, and the only thing that ever turns one is a hand — so a run that
 * presses no keys draws the first sheet for three seconds and the other seven
 * never at all. That is the same gap the whole `--scene` switch exists to
 * close, one level further in, and it is easy to miss precisely because the
 * *words* of all eight are held by `make test`: the sheet everybody checks is
 * measured and the picture beside it is not.
 *
 * Set after the state is entered rather than handed to `manual_init`, because
 * that is where the manual is born and the page it opens on for a *player* is
 * the first one, always. This is an authoring switch and moves nothing else.
 */
bool game_show_manual_page(Game *game, int page)
{
    if (game->state != STATE_MANUAL)
    {
        SDL_Log("--page needs the manual; give it --scene manual");
        return false;
    }
    if (page < 0 || page >= manual_page_count())
    {
        /* Said as a sheet number, because that is how the switch is typed and
         * how the book's own footer counts them. */
        SDL_Log("The manual has no sheet %d (1-%d)", page + 1,
                manual_page_count());
        return false;
    }
    game->presentation.manual.page = page;
    return true;
}

void game_return_to_intro(Game *game)
{
    /* Whatever this was — a finished campaign, a game over, a run abandoned
     * from the pause sheet — it is over now, and the score it reached is the
     * last thing worth keeping off it. */
    game_record_run_score(game);
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
    /* Deliberately not banked: this is the authoring entry point, not a run.
     * See the note on `load_level`. A resume comes through here too, and the
     * sector it names is by definition already in the record. */
    if (!load_level(game, level_index, LEVEL_ENTRY_AUTHORED))
    {
        SDL_Log("Could not start at sector %d; opening the title screen instead",
                level_index + 1);
        audio_play_music(&game->platform.audio, MUSIC_INTRO);
        game_enter_state(game, STATE_INTRO);
        return false;
    }

    audio_play(&game->platform.audio, SFX_MENU_START);
    return true;
}

/*
 * `--demo`, and the half of the coverage hole `--scene` could not reach.
 *
 * `--scene` opens a *screen*; this drives a *sector*. The distinction is the
 * whole reason it exists — see [demo.h](demo.h), which lists the twelve live
 * drawing functions that a keyless smoke run left executed by nothing at all.
 *
 * It refuses anything that is not a sector, for the same reason `--page`
 * refuses a run with no manual open: a switch that quietly does nothing is a
 * switch that reports a state the game is not in.
 */
bool game_start_demo(Game *game)
{
    if (game->state != STATE_LEVEL_START && game->state != STATE_PLAYING)
    {
        SDL_Log("--demo needs a sector; give it --level N");
        return false;
    }
    game->demo_active = true;
    demo_hand_init(&game->demo_hand);
    demo_grant_loadout(&game->gameplay);
    return true;
}

/*
 * `--scene NAME`, and why a shipped binary carries it.
 *
 * `make test` links no SDL, so it reaches none of the renderers; `make smoke`
 * reaches them by booting the real binary — but only into the title screen and
 * the fifteen sectors, and it never presses a key. Everything that is only
 * reached by *playing* was therefore executed by nothing in the tree at all:
 * both prologue cutscenes, the drive, the manual, the options sheet, the report
 * between sectors, the outro and the roll of names after it. That is around a
 * fifth of the presentation code, and the one undefined-behaviour bug this
 * switch was written to catch was sitting in the last of them — the screen
 * every finished run ends on, going off in every frame of it.
 *
 * It sits beside `--level N` because it is the same kind of thing: an authoring
 * and testing entry point, not a campaign path. Nothing here banks progress and
 * nothing here fabricates a state the game could not reach by playing — each
 * name is the same transition the game itself makes, taken early.
 */
bool game_start_at_scene(Game *game, const char *name)
{
    if (name == NULL)
        return false;

    static const struct
    {
        const char *name;
        GameState state;
    } SCENES[] = {
        {"abduction", STATE_ABDUCTION},
        {"drive", STATE_CHASE},
        {"arrival", STATE_OPENING_CUTSCENE},
        {"manual", STATE_MANUAL},
        {"options", STATE_SETTINGS},
        {"report", STATE_LEVEL_TRANSITION},
        {"cleared", STATE_LEVEL_CLEARED},
        {"outro", STATE_OUTRO},
        {"credits", STATE_CREDITS},
        {"continue", STATE_CONTINUE},
        {"gameover", STATE_GAME_OVER},
        {"pause", STATE_PAUSED},
    };

    /*
     * The options sheet's second page, which is a page rather than a state and
     * so cannot be a row in the table above.
     *
     * It owes a name here for exactly the reason the restroom and the manual's
     * sheets do: nothing but a hand opens it. `--scene options` draws the first
     * page, and the nine binding rows, their keycaps and the squeeze that makes
     * a long page fit the frame are drawn by nothing at all — new renderer code
     * in a file `make test` cannot link, which is the shape every bug this
     * target has ever caught has had.
     */
    if (SDL_strcmp(name, "controls") == 0)
    {
        game_open_settings(game);
        if (game->state != STATE_SETTINGS)
        {
            SDL_Log("--scene controls could not open the options sheet");
            return false;
        }
        game->settings_page = SETTINGS_PAGE_CONTROLS;
        game->settings_cursor = settings_first_row(SETTINGS_PAGE_CONTROLS);
        game->settings_bind_slot = 0;
        /* Armed, because the "PRESS A KEY" state has art of its own — an amber
         * cap and a different footer — and a run that only ever draws the
         * resting sheet has not drawn it. */
        game->settings_capturing = true;
        return true;
    }

    /*
     * The restroom is the one playable screen with no state of its own to
     * enter: it is a swap, not a transition, so it goes through the same
     * `enter_restroom` the door does rather than through the table below.
     *
     * It owes a name here for the reason everything else does. Booting a
     * sector never opens a `U`, so until the door was resolved by theme there
     * was one room behind all four and nothing in the tree ever drew it — and
     * now there are four, each a different shape, and the room's whole
     * interior is derived from its own wall bounding box. That derivation runs
     * on four sets of numbers it has never seen, in a renderer `make test`
     * cannot link.
     */
    if (SDL_strcmp(name, "restroom") == 0)
    {
        if (game->state != STATE_PLAYING && game->state != STATE_LEVEL_START)
        {
            SDL_Log("--scene restroom needs a sector; give it --level N");
            return false;
        }
        /* Asked of the sector rather than left to the fall-back, so the switch
         * cannot quietly draw the lobby's washroom for a floor that has no
         * door to it and report that as having drawn something. */
        if (!game->gameplay.level.map.has_sublevel_entrance)
        {
            SDL_Log("Sector %d has no restroom door",
                    game->campaign.current_level + 1);
            return false;
        }
        if (!enter_restroom(game))
            return false;
        game_enter_state(game, STATE_PLAYING);
        return true;
    }

    for (size_t i = 0; i < SDL_arraysize(SCENES); ++i)
    {
        if (SDL_strcmp(name, SCENES[i].name) != 0)
            continue;

        /* The three screens that read something off a run rather than standing
         * on their own. They are given the run they would have had, out of the
         * campaign state `--level` has already set up, so the screen is drawn
         * from real numbers rather than from zeroes. */
        switch (SCENES[i].state)
        {
        case STATE_SETTINGS:
            game->settings_return_state = STATE_INTRO;
            game->settings_page = SETTINGS_PAGE_MAIN;
            game->settings_cursor = settings_first_row(SETTINGS_PAGE_MAIN);
            game->settings_bind_slot = 0;
            game->settings_capturing = false;
            break;
        case STATE_PAUSED:
            game->pause_return_state = STATE_PLAYING;
            game->pause_cursor = PAUSE_ITEM_RESUME;
            break;
        case STATE_LEVEL_TRANSITION:
        {
            /* Paid here too, through the same function the stair door uses, so
             * `--scene report` draws the sheet the game would have drawn
             * rather than one with two of its fields blank. It banks nothing
             * that outlives the process, like every other `--scene`. */
            int time_bonus = 0;
            int clean_bonus = 0;
            campaign_award_sector_bonus(&game->campaign, &time_bonus,
                                        &clean_bonus);
            level_transition_init(
                &game->presentation.level_transition,
                game->campaign.current_level,
                game->campaign.current_level + 1,
                game->campaign.level_elapsed_time,
                game->campaign.score - game->campaign.level_start_score,
                gameplay_neutralized_hostiles(&game->gameplay),
                game->campaign.level_deaths,
                time_bonus, clean_bonus);
            break;
        }
        case STATE_CONTINUE:
            game->campaign.lives = 0;
            campaign_begin_continue(&game->campaign);
            break;
        default:
            break;
        }

        game_enter_state(game, SCENES[i].state);
        return true;
    }

    SDL_Log("--scene does not know '%s'", name);
    return false;
}

static void advance_level(Game *game)
{
    if ((size_t)(game->campaign.current_level + 1) < EMBEDDED_LEVEL_COUNT)
    {
        load_level(game, game->campaign.current_level + 1,
                   LEVEL_ENTRY_CAMPAIGN_STEP);
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
 * Where a saved file lives. SDL answers this per platform — Application
 * Support on macOS, AppData on Windows, XDG on Linux — and it is the only part
 * of either file that needs a platform at all, which is why it is here and not
 * in [settings.c](settings.c) or [progress.c](progress.c).
 */
static bool pref_file_path(char *out, size_t cap, const char *name)
{
    char *dir = SDL_GetPrefPath(CHUCK_APP_ORG, CHUCK_APP_NAME);
    if (dir == NULL)
        return false;
    int written = SDL_snprintf(out, cap, "%s%s", dir, name);
    SDL_free(dir);
    return written > 0 && (size_t)written < cap;
}

static bool settings_file_path(char *out, size_t cap)
{
    return pref_file_path(out, cap, "settings.cfg");
}

/* Two files rather than one, because they answer different questions: the
 * settings are what the player decided, the progress is what happened. Wiping
 * a campaign must not cost somebody their volume levels. */
static bool progress_file_path(char *out, size_t cap)
{
    return pref_file_path(out, cap, "progress.cfg");
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

    /*
     * Big enough for the whole sheet with room to spare, which it stopped being
     * the moment the bindings arrived: nine lines of `bind_weapon_next LSHIFT
     * RSHIFT` on top of the eight values and the header comment is around 450
     * bytes against the 512 this used to be. `settings_serialize` truncates
     * cleanly rather than overrunning, so the failure would have been the last
     * few bindings quietly not being saved — which is the worst shape this bug
     * can have, because it looks like the sheet forgetting one row.
     */
    char text[2048];
    size_t len = settings_serialize(&game->settings, text, sizeof(text));
    if (len == 0)
        return;
    if (!SDL_SaveFile(path, text, len))
        SDL_Log("Could not save settings: %s", SDL_GetError());
}

static void game_load_progress(Game *game)
{
    progress_defaults(&game->progress);

    char path[1024];
    if (!progress_file_path(path, sizeof(path)))
        return;

    size_t size = 0;
    void *data = SDL_LoadFile(path, &size);
    if (data == NULL)
        return; /* Nobody has played yet, which is not an error. */

    if (size > 0)
        progress_parse(&game->progress, (const char *)data);
    SDL_free(data);
    /* A file that names a sector this build does not have — an older save, or
     * a hand-edited one — must not offer a resume that cannot be loaded. */
    if ((size_t)game->progress.furthest_sector >= EMBEDDED_LEVEL_COUNT)
        game->progress.furthest_sector = (int)EMBEDDED_LEVEL_COUNT - 1;
}

static void game_save_progress(const Game *game)
{
    char path[1024];
    if (!progress_file_path(path, sizeof(path)))
        return;

    char text[256];
    size_t len = progress_serialize(&game->progress, text, sizeof(text));
    if (len == 0)
        return;
    if (!SDL_SaveFile(path, text, len))
        SDL_Log("Could not save progress: %s", SDL_GetError());
}

/*
 * The run is over, however it ended. Banking the score here rather than as it
 * is earned keeps the write off the frames the player is actually playing, and
 * **five** ways out of a campaign call it:
 *
 * - the game-over card (`game_enter_state`, STATE_GAME_OVER), banked before
 *   the card is drawn so the BEST printed on it is already this run's own;
 * - the outro (`game_enter_state`, STATE_OUTRO), the one exit that never
 *   passes through the card;
 * - `game_return_to_intro`, which is abandoning from the pause sheet and every
 *   other way of landing back on the title screen;
 * - `continue_game`, banking before the retry past the last continue — that
 *   retry zeroes the score, so it ends the run's scoring without ever reaching
 *   any of the others;
 * - `game_shutdown`, because closing the window is a way out too.
 *
 * That last one is why this comment counts them explicitly instead of saying
 * "every way out". It read "four call sites … cover every way out of a
 * campaign" long after quitting had been added as the fifth, which is the
 * failure mode a rule stated more absolutely than it is kept always has: the
 * next reader trusts it and stops looking. A sixth way out owes an entry here
 * and a call beside it.
 */
static void game_record_run_score(Game *game)
{
    if (progress_note_score(&game->progress, game->campaign.score))
        game_save_progress(game);
}

int game_resume_sector(const Game *game)
{
    return game->progress.furthest_sector;
}

int game_best_score(const Game *game)
{
    return game->progress.best_score;
}

bool game_resume_campaign(Game *game)
{
    if (game->state != STATE_INTRO || game->progress.furthest_sector <= 0)
        return false;
    /* A resume is a fresh run of that sector, not a restored one: nothing
     * about the sector's own state was ever written down. What it hands back
     * is the walk up to it, which is the part that costs an evening. */
    return game_start_at_level(game, game->progress.furthest_sector);
}

void game_open_settings(Game *game)
{
    /* The manual is on this list because the two sheets are siblings hanging
     * off the title screen: X (or J) crosses from one to the other, and the
     * options sheet then hands back to the title screen rather than to the
     * page that was open, because that is where both of them live. */
    if (game->state != STATE_INTRO && game->state != STATE_PAUSED &&
        game->state != STATE_MANUAL)
        return;
    game->settings_return_state =
        game->state == STATE_MANUAL ? STATE_INTRO : game->state;
    /* Always the first page, for the reason the pause menu always opens on
     * RESUME: a sheet that remembers where it was left is a sheet whose next
     * press of confirm lands somewhere the player did not choose. */
    game->settings_page = SETTINGS_PAGE_MAIN;
    game->settings_cursor = settings_first_row(SETTINGS_PAGE_MAIN);
    game->settings_bind_slot = 0;
    game->settings_capturing = false;
    audio_play(&game->platform.audio, SFX_MENU_PAGE);
    game_enter_state(game, STATE_SETTINGS);
}

void game_close_settings(Game *game)
{
    if (game->state != STATE_SETTINGS)
        return;
    game->settings_capturing = false;
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
    game->settings_cursor =
        settings_move_cursor(game->settings_page, game->settings_cursor, delta);
    /* The caret goes back to the first key of whatever row was arrived at: it
     * belongs to the row rather than to the sheet, and carrying it across would
     * put it on the second slot of a row the player is seeing for the first
     * time. */
    game->settings_bind_slot = 0;
    audio_play(&game->platform.audio, SFX_MENU_PAGE);
}

/* The row under the cursor, or NULL if the cursor is somehow off the page. */
static const SettingRow *settings_current_row(const Game *game)
{
    int row_count = 0;
    const SettingRow *rows = settings_rows(game->settings_page, &row_count);
    if (game->settings_cursor < 0 || game->settings_cursor >= row_count)
        return NULL;
    return &rows[game->settings_cursor];
}

static void settings_open_page(Game *game, SettingsPage page)
{
    game->settings_page = page;
    game->settings_cursor = settings_first_row(page);
    game->settings_bind_slot = 0;
    game->settings_capturing = false;
    audio_play(&game->platform.audio, SFX_MENU_PAGE);
}

bool game_settings_leave_page(Game *game)
{
    if (game->state != STATE_SETTINGS)
        return false;
    /* An armed capture is the innermost thing open, so it is what a press of
     * back puts away first: otherwise the one way out of "PRESS A KEY" would be
     * to press a key, which is the state the player is trying to escape. */
    if (game->settings_capturing)
    {
        game->settings_capturing = false;
        audio_play(&game->platform.audio, SFX_MENU_BACK);
        return true;
    }
    if (game->settings_page == SETTINGS_PAGE_MAIN)
        return false;
    settings_open_page(game, SETTINGS_PAGE_MAIN);
    return true;
}

void game_settings_confirm(Game *game)
{
    if (game->state != STATE_SETTINGS)
        return;

    const SettingRow *row = settings_current_row(game);
    if (row == NULL)
        return;

    switch (row->kind)
    {
    case SETTING_ROW_BINDING:
        /* Armed, not taken: the key itself arrives as the next press, through
         * `game_settings_capture_key`. */
        game->settings_capturing = true;
        audio_play(&game->platform.audio, SFX_MENU_PAGE);
        break;
    case SETTING_ROW_ACTION:
        if (row->id == SETTING_OPEN_CONTROLS)
        {
            settings_open_page(game, SETTINGS_PAGE_CONTROLS);
        }
        else if (row->id == SETTING_BINDINGS_RESET)
        {
            keybind_defaults(&game->settings.bindings);
            audio_play(&game->platform.audio, SFX_CARD_TARGET);
        }
        break;
    case SETTING_ROW_SLIDER:
    case SETTING_ROW_TOGGLE:
        /* ENTER is a change input on these two, which is what lets a switch be
         * flipped without the hand leaving the row it is on. */
        game_settings_adjust(game, 1);
        break;
    case SETTING_ROW_HEADING:
        break;
    }
}

/* Whether the caret is standing on one of the row's pad caps rather than one
 * of its keys, which is the whole of the difference between the two captures
 * below. */
bool game_settings_slot_is_pad(const Game *game)
{
    return game->settings_bind_slot >= BIND_PAD_SLOT;
}

bool game_settings_capture_key(Game *game, int scancode)
{
    if (game->state != STATE_SETTINGS || !game->settings_capturing)
        return false;

    /* A keyboard press while a *pad* cap is armed is not a binding and must
     * not be one — the caps are two different tables — but it is also the most
     * natural way to say "not this", so it cancels exactly as an unbindable
     * key does below. */
    if (game_settings_slot_is_pad(game))
    {
        game->settings_capturing = false;
        audio_play(&game->platform.audio, SFX_MENU_BACK);
        return true;
    }

    game->settings_capturing = false;

    const SettingRow *row = settings_current_row(game);
    BindAction action = row != NULL ? settings_row_action(row->id) : BIND_COUNT;
    if (action == BIND_COUNT)
        return true;

    /*
     * A key this sheet will not bind cancels rather than being refused with a
     * noise. That is what makes ESC the way out of a capture — it is the one
     * key a player already knows means "not this", and it is deliberately not
     * bindable precisely so that it can always mean it here.
     */
    if (!keybind_set(&game->settings.bindings, action, game->settings_bind_slot,
                     scancode))
    {
        audio_play(&game->platform.audio, SFX_MENU_BACK);
        return true;
    }
    audio_play(&game->platform.audio, SFX_CARD_TARGET);
    return true;
}

/*
 * The same thing for a button, and it is a separate entry point rather than a
 * flag on the one above because the two arrive by different routes: a key is a
 * scancode out of an SDL_EVENT_KEY_DOWN and a button is a position out of an
 * SDL_EVENT_GAMEPAD_BUTTON_DOWN, already translated into the letter it carries.
 *
 * `button` is what the file will keep — the caller has resolved the physical
 * press through `pad_capture_button` — so nothing about which pad is plugged
 * in reaches this side of the line.
 */
bool game_settings_capture_pad(Game *game, int button)
{
    if (game->state != STATE_SETTINGS || !game->settings_capturing)
        return false;

    /* And the mirror of the guard above: a pad press while a *key* cap is
     * armed cancels. Neither is an error the player made — both are somebody
     * reaching for the wrong half of a row that has both on it. */
    if (!game_settings_slot_is_pad(game))
    {
        game->settings_capturing = false;
        audio_play(&game->platform.audio, SFX_MENU_BACK);
        return true;
    }

    game->settings_capturing = false;

    const SettingRow *row = settings_current_row(game);
    BindAction action = row != NULL ? settings_row_action(row->id) : BIND_COUNT;
    if (action == BIND_COUNT)
        return true;

    /* START and BACK are what a pad has instead of ESC, so a press of either
     * lands here as an unbindable button and cancels — the same escape the
     * keyboard gets, by the same mechanism rather than by a special case. */
    if (!keybind_set_pad(&game->settings.bindings, action,
                         game->settings_bind_slot - BIND_PAD_SLOT, button))
    {
        audio_play(&game->platform.audio, SFX_MENU_BACK);
        return true;
    }
    audio_play(&game->platform.audio, SFX_CARD_TARGET);
    return true;
}

void game_settings_adjust(Game *game, int delta)
{
    if (game->state != STATE_SETTINGS)
        return;

    int row_count = 0;
    const SettingRow *rows = settings_rows(game->settings_page, &row_count);
    if (game->settings_cursor < 0 || game->settings_cursor >= row_count)
        return;

    /* On a binding row the change inputs move the caret between the row's two
     * keys rather than changing anything: this is the row whose value is taken
     * by pressing it, not by pushing at it. */
    if (rows[game->settings_cursor].kind == SETTING_ROW_BINDING)
    {
        int next = game->settings_bind_slot + (delta > 0 ? 1 : -1);
        /* Four caps rather than two: the row's keys, then the row's pad
         * buttons. One caret walks all of them, so the pad is a pair of slots
         * on the same row instead of a page of its own — which is what keeps
         * "what does JUMP answer to" a single line of the sheet however the
         * player is holding the game. */
        if (next < 0 || next >= BIND_TOTAL_SLOTS)
            return; /* Already at an end: nothing moved, so nothing clicks. */
        game->settings_bind_slot = next;
        audio_play(&game->platform.audio, SFX_MENU_PAGE);
        return;
    }

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
    case SETTING_REDUCED_MOTION:
        /* The strobes are read where they are drawn, but a shake already in
         * flight is state rather than a draw, so it is put down here: switching
         * this on in the middle of one and watching the frame carry on shaking
         * is the sheet not doing what it says. */
        game->presentation.camera_shake_x = 0.0f;
        game->presentation.camera_shake_y = 0.0f;
        break;
    case SETTING_CRT_FILTER:
    case SETTING_INFINITE_LIVES:
    case SETTING_NONE:
        /* Read where they are used: the finishing pass at the bottom of
         * game_render, and the death that would have cost a life. */
        break;
    case SETTING_OPEN_CONTROLS:
    case SETTING_BINDINGS_RESET:
    case SETTING_BIND_FIRST:
        /* Not values: `settings_adjust` already refused them above, so this is
         * unreachable and is listed so that a tenth setting cannot be added
         * without this switch being looked at. */
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
        intro_init(&game->presentation.intro, win_w, win_h,
                   game_pad_hints(game), game_resume_sector(game));
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
        /* A finished campaign is a finished run, and this is the only way out
         * of one that never passes through the game-over card. */
        game_record_run_score(game);
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
        /* Banked before the card is drawn, so the BEST on it is already this
         * run's own score when this run is the best one. */
        game_record_run_score(game);
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
        /* The hold is only ever answered from the title screen, which is the
         * one screen the player did not open and so the one place quitting is
         * not "backing out" of anything. */
        if (intro_update(&game->presentation.intro, dt, win_w, win_h, mx, my,
                         game_pad_hints(game), game_resume_sector(game),
                         game->input.cancel_held))
        {
            game->quit_requested = true;
        }

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

    /*
     * The floor pays before anything is decided about where the player goes
     * next, because all three ways out of a sector come through here and only
     * one of them draws a report: the stair door reports, the window onto a
     * climb hands straight over, and the last sector goes to the outro. Banked
     * inside the reporting branch — which is where it is easiest to put — the
     * four climbs and the roof would have been the five floors in the campaign
     * that paid nothing for being cleared quickly.
     */
    int time_bonus = 0;
    int clean_bonus = 0;
    campaign_award_sector_bonus(&game->campaign, &time_bonus, &clean_bonus);
    /* And the threshold is looked at here rather than left to the next
     * sector's first frame: a bonus is the largest single jump the score ever
     * makes, so it is the most likely thing in the game to buy a life, and a
     * 1UP that arrives a screen later has arrived for no reason the player can
     * see. */
    while (campaign_check_extra_life(&game->campaign))
    {
        game_events_sound(&game->gameplay.events, SFX_PICKUP_HEALTH);
        game->presentation.extra_life_timer = 2.5f;
    }

    if ((size_t)(game->campaign.current_level + 1) < EMBEDDED_LEVEL_COUNT)
    {
        int next_level = game->campaign.current_level + 1;
        if (game->gameplay.level.map.has_window)
        {
            /* A window is a continuous physical route between inside and the
             * facade. The hostage/elevator report belongs only between normal
             * interior sectors and would contradict what is on screen here. */
            audio_stop_music(&game->platform.audio);
            if (!load_level(game, next_level, LEVEL_ENTRY_CAMPAIGN_STEP))
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
                game->campaign.level_deaths,
                time_bonus, clean_bonus);
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

    gameplay_combat_update_enemy_bullets(&game->gameplay, &game->campaign,
                                         dt);

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
    /*
     * And then thrown away again, if this is a demo run.
     *
     * After the read rather than instead of it, because the read is also what
     * polls the pad and keeps the hot-plug state honest, and because a demo
     * that skipped it would be the one code path in the game where
     * `game_read_input` is not exercised. It only replaces the input on the
     * frames a sector is actually being simulated: the title screen and the
     * cutscenes have their own script — the dwell in `tools/smoke.sh` — and a
     * hand mashing confirm through them would skip the very screens that dwell
     * exists to sit on.
     */
    if (game->demo_active &&
        (game->state == STATE_PLAYING || game->state == STATE_LEVEL_START))
    {
        demo_hand_drive(&game->demo_hand, &game->gameplay, &game->input, dt);
    }
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
    /* Closing the window is the fifth way a run ends, and for a long time it
     * was the one that did not count. The other four bank the score
     * (`game_record_run_score`), so abandoning from the pause sheet kept a
     * record and Cmd-Q one keystroke later threw the same run away — two
     * answers to the same question, and the losing one is whichever the
     * player happened to reach for. The write is already happening here for
     * the settings, so it costs nothing to be honest about it. */
    game_record_run_score(game);
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
