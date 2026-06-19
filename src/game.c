#include "game.h"
#include "camera.h"
#include "embedded_levels.h"
#include "gameplay_ai.h"
#include "gameplay_climb.h"
#include "gameplay_combat.h"
#include "gameplay_interaction.h"
#include "gameplay_physics.h"
#include "gameplay_world.h"
#include "intel.h"
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
/* Declared up here because `game_soak_screen` opens the controls page for
 * `--screen settings --page 2`, and the definition sits with the rest of the
 * options sheet a long way below it. */
static void settings_open_page(Game *game, SettingsPage page);

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

/* Hand the shell-owned assist choices to a simulation as plain flags. The
 * gameplay core stays deterministic and never knows a menu exists. */
static void apply_assist_to_state(Game *game, GameplayState *state)
{
    /*
     * The run remembers that an assist was on, and this is the place that tells
     * it, because this is the one function every assist change already passes
     * through — the load of a sector, the restroom's own simulation, and
     * `game_apply_assist_everywhere` when a switch is flipped mid-run. Noting it
     * at the switch instead would be a fourth call site to keep in step, and the
     * flag only ever sets, so being told twice costs nothing.
     */
    campaign_note_assist(&game->campaign, settings_assist_any(&game->settings));
    /* And the other lever, for the same reason and by the same route: the
     * campaign has to know, because a continue hands out lives and the number
     * it hands out is the mode's. Not sticky like the assist above — this one
     * is live difficulty and follows the sheet in both directions. */
    campaign_note_veteran(&game->campaign, game->settings.challenge.veteran);
    state->assist_slow_enemies = game->settings.assist.slower_guards;
    state->assist_more_hearts = game->settings.assist.more_hearts;
    state->veteran = game->settings.challenge.veteran;
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
 * Exchange the contents of two objects of the same type, a cache line at a
 * time.
 *
 * `tmp = a; a = b; b = tmp;` is what this replaced and what it still means. The
 * objects here are `GameplayState`, which is 68KB — a `LevelMap` holds its grid
 * inline — so the obvious form put a third one of those on the stack and gave
 * `swap_gameplay_areas` a 70KB frame, which `game_update` then inherited by
 * inlining it. Nothing was ever at risk of overflowing on 8MB of main thread,
 * and the copying is a few hundred kilobytes once per door: this is tidiness
 * rather than a fix, and it is worth the four lines because the alternative is
 * a per-frame function whose frame is dominated by a branch that runs when
 * somebody opens a lavatory.
 */
static void swap_objects(void *a, void *b, size_t size)
{
    unsigned char *left = a;
    unsigned char *right = b;
    unsigned char scratch[64];
    while (size > 0)
    {
        size_t chunk = size < sizeof(scratch) ? size : sizeof(scratch);
        memcpy(scratch, left, chunk);
        memcpy(left, right, chunk);
        memcpy(right, scratch, chunk);
        left += chunk;
        right += chunk;
        size -= chunk;
    }
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
    swap_objects(&game->gameplay, &game->inactive_gameplay,
                 sizeof(game->gameplay));
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
    float travelling_mercy = game->gameplay.invuln_timer;
    game->main_level_cam_x = game->presentation.cam_x;
    game->main_level_cam_y = game->presentation.cam_y;
    swap_gameplay_areas(game);
    gameplay_carry_through_doorway(&game->gameplay, &travelling_player,
                                   travelling_mercy);
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
    float travelling_mercy = game->gameplay.invuln_timer;
    swap_gameplay_areas(game);
    gameplay_carry_through_doorway(&game->gameplay, &travelling_player,
                                   travelling_mercy);
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

    /* Nothing was cleared to get here, so nothing is owed a tally. Only a step
     * from one sector to the next carries one, and `try_finish_current_level`
     * has already set it by the time it calls this; a run starting or an
     * author dropping in must not inherit the last one drawn. */
    if (entry != LEVEL_ENTRY_CAMPAIGN_STEP)
        sector_tally_clear(&game->presentation.sector_tally);

    if (index < 0 || (size_t)index >= EMBEDDED_LEVEL_COUNT)
    {
        /* Said as a sector number, because that is the only way a sector is
         * ever named anywhere a human reads one — `--level N`, the strip's
         * SECTOR field, the debug picker and the editor's playtest button are
         * all 1-based. This line used to print the 0-based array index beside
         * `game_start_at_level`'s 1-based one, so a mistyped `--level 99` was
         * answered by two numbers, neither of them together and one of them
         * never typed. */
        /* The count is cast rather than printed with `%zu`. It is a `size_t`
         * and `%zu` is what C says, and SDL's own printf implements it — but the
         * format attribute SDL puts on `SDL_Log` is checked by gcc against the
         * *platform's* printf, and on Windows that archetype has never heard of
         * `z`. So a correct line produced two warnings on the mingw build and
         * none on the clang one. A campaign length fits an `int` with room for
         * every tower anybody will build. */
        SDL_Log("Sector %d is outside the campaign (1-%d)", index + 1,
                (int)EMBEDDED_LEVEL_COUNT);
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
    campaign_reset(&game->campaign, game->settings.challenge.veteran);
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

bool game_init(Game *game, GameRunKind run)
{
    return game_init_seeded(game, (uint64_t)time(NULL), run);
}

bool game_init_seeded(Game *game, uint64_t seed, GameRunKind run)
{
    SDL_zerop(game);
    /* Before the two loads below, because it is what decides whether they read
     * anything at all — see `pref_file_path`. */
    game->platform.scripted = run == GAME_RUN_SCRIPT;
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
    if (!SDL_CreateWindowAndRenderer("Chuck", VIEW_W, VIEW_H,
                                     SDL_WINDOW_RESIZABLE,
                                     &game->platform.window, &game->platform.renderer))
    {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }

    /* Use a fixed logical presentation so the game's coordinate system stays
     * consistent when the window is resized or when toggling fullscreen.
     * This makes the game look identical but scaled when entering fullscreen. */
    SDL_SetRenderLogicalPresentation(game->platform.renderer, VIEW_W, VIEW_H,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

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

    /* SDL's own stream off the same seed as the simulation's, rather than off
     * the wall clock beside it. The camera shake, the particles and the title
     * screen's starfield draw from this one, so a run pinned by `--seed` was
     * still not repeatable while these two disagreed about where randomness
     * comes from — and repeatability is the whole of what that switch is for.
     * An ordinary run is seeded from `time(NULL)` through `game_init`, which is
     * as arbitrary as the tick count it replaces. */
    SDL_srand(seed);

    campaign_reset(&game->campaign, game->settings.challenge.veteran);
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

    /*
     * The window was created windowed; if the file says otherwise this is
     * where it is put right, once the renderer's logical presentation is set
     * so the first fullscreen frame is already letterboxed correctly.
     *
     * A scripted run stays windowed whatever the file said, and that is not
     * tidiness — it is what makes `--shot` write the same picture on every
     * machine. `screenshot_write` reads back the render target, which under
     * letterbox presentation is the *window*: fullscreen, the capture comes out
     * at the display's size with the logical frame scaled into it by whatever
     * non-integer factor that display implies, and pixel art does not survive
     * one of those. Windowed, the window is VIEW_W x VIEW_H and the capture is
     * the logical frame exactly. `tools/soak.sh` holds it.
     */
    if (game->settings.fullscreen && !game->platform.scripted)
        game_set_fullscreen(game, true);

    /* Initialise particle system */
    particle_system_init(&game->presentation.particles);

    /* Boot straight to the title screen; the prologue's three beats — the
     * kerb, the drive, the tower's front door — play after START. */
    game_enter_state(game, STATE_INTRO);

    game->platform.last_tick = SDL_GetTicksNS();
    return true;
}

/*
 * Open the field manual, and remember where to put it back.
 *
 * **It used to be the title screen and nothing else, on the stated grounds that
 * there is "no simulation to pause".** That reason did not survive the sheet
 * beside it: `game_open_settings` opens from `STATE_PAUSED` and hands back to it,
 * so the machinery for a sheet over a paused run already existed and was already
 * in use. What the restriction actually cost was the one moment the manual is
 * for — a player stuck on a floor, wondering what the flash charge does or how a
 * body is hauled, had to abandon the run to read the sheet that explains it. Ten
 * sheets that exist to be read, unreachable from inside the thing they describe.
 *
 * So it opens from the pause sheet too, which is also a row on that sheet now,
 * and `game_close_manual` puts it back where it came from. `settings_return_state`
 * is the same idea and this is deliberately spelled the same way.
 */
void game_open_manual(Game *game)
{
    if (game->state != STATE_INTRO && game->state != STATE_PAUSED)
        return;
    game->manual_return_state = game->state;
    /* The sheaf itself is laid out and put back on its first sheet by
     * `manual_init`, which `game_enter_state` already runs on the way into
     * `STATE_MANUAL` — for the reason the pause menu always opens on RESUME and
     * the options sheet always opens on its first page. */
    audio_play(&game->platform.audio, SFX_MENU_PAGE);
    game_enter_state(game, STATE_MANUAL);
}

/*
 * Put the manual away, back to whatever it was opened over.
 *
 * Every "done" on every input path came through `game_return_to_intro` before
 * this existed, which was right while the title screen was the only way in and
 * became a bug the moment the pause sheet was another: that function banks the
 * run's score and ends it, so closing the manual mid-sector would have abandoned
 * the run it was opened from.
 */
void game_close_manual(Game *game)
{
    if (game->state != STATE_MANUAL)
        return;
    if (game->manual_return_state == STATE_PAUSED)
    {
        audio_play(&game->platform.audio, SFX_MENU_BACK);
        game_enter_state(game, STATE_PAUSED);
        return;
    }
    game_return_to_intro(game);
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
    campaign_reset(&game->campaign, game->settings.challenge.veteran);
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
 * Put the game on one named screen and leave it there, for `--screen NAME`.
 *
 * This exists because `tools/soak.sh` was claiming coverage it did not have,
 * and the claim was written down in the script's own header: "the title screen
 * first ... it is the only entry point that reaches `intro.c`, the prologue's
 * cutscenes and the attract music". The first third of that was true and the
 * rest never was. `STATE_INTRO` advances on `game->input.confirm` and nothing
 * else, and every line that sets that flag is inside an SDL event handler — a
 * key going down or a pad button going down. A headless soak has no window and
 * receives no events, so it sat on the title screen for its whole budget and
 * the prologue was never drawn. Forty seconds of it proves the point: the
 * process reports finishing without ever having left the first screen.
 *
 * What that left sanitizer-compiled and never sanitizer-executed is the same
 * list `soak.sh` was written to close, one floor up: the abduction, the drive
 * and the opening cutscene, so `chase_render.c` entire; the report between
 * sectors, the outro and the roll of names, so most of `cutscene.c`;
 * `manual.c`; the settings sheet, the pause sheet, the continue prompt and the
 * game-over card in `game_render.c`; and the four restroom sublevels, which no
 * `--level N` run has ever entered either.
 *
 * A switch rather than synthesised input, for the reason `--soak` is a switch
 * rather than a `kill`: a soak that pushed fake events would be testing the
 * event handlers' idea of the state machine instead of the renderers, and a
 * screen reached by three simulated keypresses is a screen whose coverage
 * breaks the day a menu gains a row. Naming the state is the thing that stays
 * true.
 *
 * The screens that draw over a live sector load one first, because that is
 * what the renderer does: the pause sheet, the report, the continue prompt and
 * the game-over card are all drawn on top of `render_world`, and entering one
 * with no level loaded would soak a black frame and call it covered.
 */
/*
 * A sector a few seconds after it went wrong, staged rather than played.
 *
 * **Why this exists.** `--screen` reaches everything behind a menu choice, a
 * cleared sector or a finished campaign, and `--level N` reaches every sector —
 * but a headless run makes no input, so the player it draws is a man standing
 * still on the spot he spawned. Measured with a coverage build, that left a list
 * of drawing functions the sanitizers compile and never execute, and it is not a
 * list of edge cases: `draw_downed_enemy` and `draw_downed_dog` are the bodies
 * the entire quiet route is played around, `level_art_broken_wall_tile` is what
 * a `%` looks like once it is open, `render_alarm_lighting` is what the building
 * looks like when it knows, `particle_system_emit` and
 * `particle_system_explosion` are the whole emitting half of `particle.c`, and
 * the bazooka and its rocket are four functions between them. Every one of them
 * is on the far side of the SDL line, so no test in the suite can reach them
 * either. They were covered by nothing at all.
 *
 * **Why staged and not driven.** `soak.sh` argues that a screen reached by fake
 * button presses is a screen whose coverage breaks the day a menu gains a row,
 * and that what would be under test is the event handlers rather than the
 * renderers. That argument is about *reaching* a screen and it holds. This is the
 * same answer one layer in: name a *world state* the way `report` and `cleared`
 * already name theirs — both of those call `level_transition_init` and
 * `sector_tally_set` with numbers a real clear would produce — and let the
 * renderers draw it. Nothing here simulates; it writes the state a fight leaves
 * behind and hands it to the frame.
 *
 * **The sector is found, not written down.** It needs a `%` to open and a dog to
 * put down, so it scans the campaign for the first sector with both rather than
 * naming one — a map edit that moves them is a map edit this still works after.
 * `--level N` overrides it for anyone pointing this at a particular floor.
 *
 * **The poses are pages**, because `draw_player` answers hacking first, crawling
 * second and everything else after, so one frame can hold exactly one of them —
 * the same "one screen name, more than one drawing" that `--page` already
 * answers for the manual's sheaf and the options sheet's two halves.
 */
/*
 * The first sector with both a `%` to open and a dog to put down.
 *
 * Derived rather than named, for the reason the sweep counts the campaign's
 * length rather than writing it down: an aftermath needs those two features and a
 * literal sector number here would be the copy that went stale the first time a
 * map moved. Nought — the lobby — if nothing in the campaign has both, which the
 * caller then reports rather than staging half a screen.
 */
static int soak_aftermath_sector(void)
{
    for (size_t i = 0; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        static Level probe;
        Rng rng;
        rng_seed(&rng, 20250818u + (uint64_t)i);
        if (!level_load_data(&probe, EMBEDDED_LEVELS[i].name,
                             EMBEDDED_LEVELS[i].data, EMBEDDED_LEVELS[i].size,
                             &rng))
            continue;
        if (probe.map.mode != LEVEL_MODE_INTERIOR)
            continue;

        bool has_dog = false;
        for (int e = 0; e < probe.map.enemy_count && !has_dog; ++e)
            has_dog = probe.map.enemy_spawns[e].has_dog;
        if (!has_dog)
            continue;

        for (int row = 0; row < probe.map.height; ++row)
            for (int col = 0; col < probe.map.width; ++col)
                if (probe.map.tiles[row][col] == TILE_WEAK_WALL)
                    return (int)i;
    }
    return 0;
}

/*
 * The first boundary in the campaign where the tally actually rides a reveal.
 *
 * Only a sector that leaves by a window gets one: `try_finish_current_level`
 * sends every other floor to the report, and the report clears the band. So the
 * frame this screen exists to draw is the reveal of the sector *after* a window,
 * and staging it anywhere else is a picture of something the game never shows —
 * which is the same lesson `cleared` learned by being staged on sector 1.
 *
 * Derived rather than written down, for the reason `soak_aftermath_sector` is:
 * which sectors leave by a window is a property of the maps and has moved once
 * already.
 */
static int soak_reveal_sector(void)
{
    for (size_t i = 1; i < EMBEDDED_LEVEL_COUNT; ++i)
    {
        static Level probe;
        Rng rng;
        rng_seed(&rng, 20260822u + (uint64_t)i);
        if (!level_load_data(&probe, EMBEDDED_LEVELS[i - 1].name,
                             EMBEDDED_LEVELS[i - 1].data,
                             EMBEDDED_LEVELS[i - 1].size, &rng))
            continue;
        if (probe.map.has_window)
            return (int)i;
    }
    return 1;
}

/*
 * Point the camera at the staged frame and stop the clock, which both halves of
 * `soak_stage_aftermath` end with and neither may skip.
 *
 * The camera, because the tile loops in `render_world` draw only what is inside
 * the viewport and nothing moves it while paused — a frame staged without this
 * looks at the top-left corner of the map, which is what hid the opened patch on
 * this screen's second outing.
 *
 * The clock, because the simulation's first step clears every transient the
 * staging just wrote: nothing is holding the crouch, so `crawling` goes false,
 * and `action_timer` runs out inside a tenth of a second. `STATE_PAUSED` is the
 * one state whose own comment is "time stands still", and it draws `render_world`
 * underneath its sheet, so the staged frame is the frame that gets rasterized.
 * `pause_return_state` is set because `game_toggle_pause` is not what put us
 * here, and a sheet that resumes into nothing is a bad state for the sanitizers
 * to walk out through.
 */
static bool soak_freeze_staged_frame(Game *game)
{
    /* Said out loud to the renderer: the state that freezes the frame is also the
     * state that draws a menu over it. See `PlatformState.staged_frame`. */
    game->platform.staged_frame = true;
    snap_camera_to_player(game);
    game->pause_return_state = STATE_PLAYING;
    game->pause_cursor = PAUSE_ITEM_RESUME;
    game->pause_abandon_armed = false;
    game_enter_state(game, STATE_PAUSED);
    return true;
}

static bool soak_stage_aftermath(Game *game, int page)
{
    GameplayState *g = &game->gameplay;

    /*
     * A page this screen has no pose for is refused rather than rounded down to
     * the first one, which is what the `default` arm below would otherwise do.
     * The manual has said this about its own sheets since `--page` existed; the
     * aftermath did not, so `--page 99` drew pose 1 and the sweep logged
     * `aftermath pose 99 ok` — a check reporting coverage it does not have, on
     * the switch written to end exactly that. The one caller is a script and a
     * script is owed an exit code.
     */
    if (page > AFTERMATH_POSE_COUNT)
    {
        SDL_Log("Pose %d is outside the aftermath's %d", page,
                AFTERMATH_POSE_COUNT);
        return false;
    }

    /*
     * The map, all of it. `load_level` leaves the reveal at its first tile and an
     * ordinary run spends `STATE_LEVEL_START` walking it out — so a jump straight
     * to `STATE_PLAYING` draws a mostly hidden floor, which is what this screen
     * did on its first outing: `render_world` ran at six percent and every figure
     * behind the reveal stayed unexecuted. `enter_restroom` finishes the reveal
     * the same way and for the same reason.
     */
    level_reveal_init(&g->level);
    level_reveal_step(&g->level, 1000.0f);

    /* On its feet and in the fight: the spawn pass runs at `STATE_LEVEL_START`
     * in an ordinary run, and nothing has run it here. A climb spawns nobody, and
     * the branch below is what it gets instead. */
    if (g->level.map.mode != LEVEL_MODE_FACADE)
        gameplay_ai_spawn_level_entities(g);

    /*
     * A climb has none of what follows — no guards, no dogs, no `%` — and what it
     * does have is thrown objects and birds, which spawn on a timer and were
     * therefore covered by whether a two-second window happened to catch one.
     * "Sometimes executed" is not a coverage claim, so a facade stages its own
     * hazards and returns.
     */
    if (g->level.map.mode == LEVEL_MODE_FACADE)
    {
        ThrownObject *thrown = &g->thrown_objects[0];
        thrown->active = true;
        thrown->x = g->player.x + 70.0f;
        thrown->y = g->player.y - 20.0f;
        thrown->vx = -180.0f;
        thrown->vy = 40.0f;
        thrown->angle = 0.8f;
        thrown->variant = 1;

        Bird *bird = &g->birds[0];
        bird->active = true;
        bird->x = g->player.x - 80.0f;
        bird->y = g->player.y - 40.0f;
        bird->vx = 150.0f;
        bird->vy = 0.0f;
        bird->anim_time = 0.4f;

        /* The gust, which is the one state the wall's own HUD strip reports. */
        g->facade_wind_phase = FACADE_WIND_GUSTING;
        g->facade_wind_timer = 1.0f;
        g->facade_wind_dir = 1;

        particle_system_dust(&game->presentation.particles,
                             g->player.x, g->player.y, 4, 1.0f);

        return soak_freeze_staged_frame(game);
    }

    /*
     * A body, and a dog beside it. The first of each, because which one it is
     * decides nothing about what gets drawn.
     *
     * **Scored and tallied as well as laid out**, which is the half this
     * staging did without for as long as it existed. Two men and a dog are
     * `2 * ENEMY_SCORE + DOG_SCORE`, and writing `dead = true` by hand skips
     * the path that says so — so the strip above the bodies read `SCORE
     * 0000000`, on a floor with two corpses and a blown wall on it. That is
     * the impossible pair this file has already corrected three times on the
     * tally band's own fixture (the elapsed-and-best, the docket over sector
     * one, the hostile count on a two-man floor) and never once here, on the
     * one staging that actually **ships**: `02-alarm` and `13-duct` in
     * [tools/press_kit.sh](../tools/press_kit.sh) are both this screen, and a
     * seven-digit field sitting at nought beside the bodies it was paid for is
     * a frame the game cannot produce. A fixture with a stated rule owes that
     * rule to every field in it — and every assertion this screen has is a
     * width, which a wrong number is exactly as wide as a right one.
     *
     * Through the same tally `gameplay_record_neutralized` keeps, so the crew
     * situation the net reads agrees with the floor too.
     */
    int downed_men = 0;
    for (int i = 0; i < g->enemy_count && downed_men < 2; ++i)
    {
        g->enemies[i].dead = true;
        gameplay_record_neutralized(g, &game->campaign);
        game->campaign.score += ENEMY_SCORE;
        downed_men++;
    }
    int downed_dogs = 0;
    for (int i = 0; i < g->dog_count && downed_dogs < 1; ++i)
    {
        g->dogs[i].dead = true;
        gameplay_record_neutralized(g, &game->campaign);
        game->campaign.score += DOG_SCORE;
        downed_dogs++;
    }

    /*
     * A patch somebody put a charge through. `wall_broken` is runtime state that
     * outlives a lost life, so writing it is exactly what a blast does.
     *
     * The *nearest* patch to where the player is standing, not the first one the
     * scan meets: the wall loop in `render_world` only draws the tiles inside the
     * viewport, so opening a patch four storeys up covers nothing. That is how
     * this screen came to stage a broken wall nobody could see on its first
     * outing.
     */
    int opened = 0;
    int best_row = 0;
    int best_col = 0;
    float best_d2 = 0.0f;
    for (int row = 0; row < g->level.map.height; ++row)
        for (int col = 0; col < g->level.map.width; ++col)
        {
            if (g->level.map.tiles[row][col] != TILE_WEAK_WALL)
                continue;
            float dx = (float)col * TILE_SIZE - g->player.x;
            float dy = (float)row * TILE_SIZE - g->player.y;
            float d2 = dx * dx + dy * dy;
            if (opened == 0 || d2 < best_d2)
            {
                best_d2 = d2;
                best_row = row;
                best_col = col;
            }
            opened++;
        }
    if (opened > 0)
    {
        g->level.runtime.wall_broken[best_row][best_col] = true;

        /*
         * And the man is moved to the patch rather than the patch chosen near the
         * man, which is the other half of the same lesson.
         *
         * Sector 4 is the first floor in the campaign with both a dog and a `%`,
         * and its patch is twenty-eight tiles from the spawn — a viewport is
         * twenty-five wide, so "the nearest patch to the player" was still off
         * screen and the tile loop skipped it. Standing him beside it puts the
         * opened wall, the bodies and the alarm in one frame, which is also what
         * the scene is supposed to be: where the fight happened.
         *
         * A tile with air at head height and something solid under it, searched
         * outward from the patch, so the figure is standing rather than floating.
         * If the floor plan offers none, he stays where he spawned and the patch
         * simply is not in shot — a staged scene is worth less than a wrong one.
         */
        for (int reach = 1; reach <= 3; ++reach)
        {
            bool placed = false;
            for (int side = -1; side <= 1 && !placed; side += 2)
            {
                int col = best_col + side * reach;
                if (col < 1 || col >= g->level.map.width - 1)
                    continue;
                for (int row = best_row; row < g->level.map.height - 1; ++row)
                {
                    if (level_is_solid(&g->level, col, row))
                        break;
                    if (!level_is_solid(&g->level, col, row + 1))
                        continue;
                    g->player.x = (float)col * TILE_SIZE +
                                  (TILE_SIZE - PLAYER_W) * 0.5f;
                    g->player.y = (float)(row + 1) * TILE_SIZE - PLAYER_H;
                    g->player.facing = -side;
                    placed = true;
                    break;
                }
            }
            if (placed)
                break;
        }
    }

    /* The building knows. This is the one flag `render_alarm_lighting` reads. */
    g->terminal_alarm_timer = ALARM_CALM_TIME;

    /*
     * And the crawl goes into a duct, because page 2 is the crawl and a crawl
     * staged on a corridor floor is the pose without the one tile the pose
     * exists for. `render_duct_fronts` is the pass that lays a shaft's louvres
     * back over the man inside it, and nothing else in the game reaches it: a
     * headless run presses nothing, so no sweep has ever put Chuck in trunking.
     *
     * Here rather than in the pose switch below, so the particles, the rocket in
     * the air and the camera all follow him in. Staged after them it would have
     * drawn a crawler in a shaft with his own smoke a storey away.
     *
     * The middle of the longest run, so there is trunking either side of him and
     * the figure is inside rather than at a mouth — and the slab under that tile
     * is checked rather than assumed, because a duct over air is the one shape
     * the editor refuses and the one this would stage as a man in mid-fall. A
     * sector with no trunking on it — or a run whose middle tile somehow has
     * nothing under it — stages the crawl where it always did.
     */
    if (page == 2)
    {
        int duct_row = -1;
        int duct_col = -1;
        int longest = 0;
        for (int row = 0; row < g->level.map.height; ++row)
        {
            int run = 0;
            for (int col = 0; col <= g->level.map.width; ++col)
            {
                if (col < g->level.map.width &&
                    g->level.map.tiles[row][col] == TILE_VENT)
                {
                    run++;
                    continue;
                }
                if (run > longest)
                {
                    longest = run;
                    duct_row = row;
                    duct_col = col - (run + 1) / 2;
                }
                run = 0;
            }
        }
        if (duct_row >= 0 && level_is_solid(&g->level, duct_col, duct_row + 1))
        {
            g->player.x = (float)duct_col * TILE_SIZE +
                          (TILE_SIZE - PLAYER_W) * 0.5f;
            g->player.y = (float)(duct_row + 1) * TILE_SIZE - PLAYER_CRAWL_H;
            g->player.facing = 1;
        }
    }

    /*
     * And the rung poses go onto a rung, for the reason the crawl goes into a
     * duct: `draw_player` takes its climbing branch off `on_ladder`, which is a
     * flag rather than a place, so setting it where the man is standing drew him
     * gripping a ladder that was not there. Page 4 has done that since it was
     * written and nothing said so — a counter cannot tell a frame that was drawn
     * from a frame anybody could read, and until `--shot` existed nobody had
     * looked at this one.
     *
     * Before the particles and the rocket below, so his own smoke follows him,
     * which is the same ordering the duct block above is careful about.
     *
     * The rung *nearest where he already is*, not the first one the scan meets,
     * and for the same reason the patch above is chosen that way: this screen is
     * one frame of where the fight happened, so the bodies, the opened wall and
     * the alarm have to stay in shot with him. Taking the first ladder on the map
     * walked him to the far corner of the floor and left the scene behind — which
     * is the mistake the `%` search already has a paragraph about, made again one
     * block down. A rung with another above it, so he is on a run rather than on
     * its last tile; a floor with no ladder on it stages the pose where it always
     * did.
     */
    if (page == 4 || page == 6 || page == 7)
    {
        int rung_row = -1;
        int rung_col = -1;
        float best_rung = 0.0f;
        for (int row = 1; row < g->level.map.height - 1; ++row)
            for (int col = 1; col < g->level.map.width - 1; ++col)
            {
                if (!level_is_ladder(&g->level, col, row) ||
                    !level_is_ladder(&g->level, col, row - 1))
                {
                    continue;
                }
                float dx = (float)col * TILE_SIZE - g->player.x;
                float dy = (float)row * TILE_SIZE - g->player.y;
                float d2 = dx * dx + dy * dy;
                if (rung_row < 0 || d2 < best_rung)
                {
                    best_rung = d2;
                    rung_row = row;
                    rung_col = col;
                }
            }
        if (rung_row >= 0)
        {
            g->player.x = (float)rung_col * TILE_SIZE +
                          (TILE_SIZE - PLAYER_W) * 0.5f;
            g->player.y = (float)rung_row * TILE_SIZE;
            g->player.facing = 1;
        }
    }

    /* Smoke, sparks and dust, which is the emitting half of `particle.c` and the
     * half no soak had ever called. */
    float px = g->player.x + PLAYER_W * 0.5f;
    float py = g->player.y + PLAYER_H * 0.5f;
    particle_system_emit(&game->presentation.particles, px, py, 6,
                         g->player.facing);
    particle_system_explosion(&game->presentation.particles, px + 24.0f, py, 10);
    particle_system_dust(&game->presentation.particles, px, py + 8.0f, 4, 1.0f);

    /* And the tube in his hands, mid-shot, with the round still in the air. An
     * `action_timer` past `PLAYER_MUZZLE_FLASH_TIME` is also what puts the muzzle
     * flash on, which is why this stages a shot's own duration rather than a
     * number picked to look right. */
    g->player.active_weapon = PLAYER_WEAPON_BAZOOKA;
    /* Loaded, because `draw_player` reads the tube as drawn only when there is a
     * round in it (`bazooka_rockets > 0`) or a shot in flight from it — an empty
     * launcher is not held like a launcher. Without this the weapon was selected
     * and no bazooka was drawn. */
    g->player.bazooka_rockets = 1;
    g->player.bazooka_firing = true;
    g->player.action_timer = PLAYER_SHOT_ACTION_TIME;
    g->player.knife_attacking = false;
    g->player.grenade_throwing = false;
    Rocket *rocket = &g->rockets[0];
    rocket->active = true;
    rocket->x = px + 40.0f;
    rocket->y = py;

    switch (page)
    {
    case 2:
        /* Flat on the floor: `draw_player` answers `crawling` before it reaches
         * a weapon, so this page is the crawl and nothing else. */
        g->player.crawling = true;
        rocket->vx = -260.0f;
        rocket->vy = 0.0f;
        break;
    case 3:
        /* At a console. Answered first of all, so this page is the hack. */
        g->terminal_hacking = true;
        g->terminal_hack_progress = 0.5f;
        rocket->vx = 260.0f;
        rocket->vy = 0.0f;
        break;
    case 4:
        /* On a rung, firing up the shaft: the one route to
         * `draw_vertical_bazooka_weapon`, which lives in the climbing branch and
         * wants a vertical aim, and to `draw_vertical_rocket_sprite` with it. */
        g->player.on_ladder = true;
        g->player.shot_vertical = -1;
        rocket->vx = 0.0f;
        rocket->vy = -260.0f;
        break;
    case 5:
        /*
         * The sidearm rather than the tube, which is the only route to
         * `draw_muzzle_flash`: `draw_player` takes the bazooka branch whenever
         * one is in hand, so the pose that covers the launcher is exactly the
         * pose that stops covering the flash. Two poses, because one frame cannot
         * hold both hands.
         */
        g->player.active_weapon = PLAYER_WEAPON_PISTOL;
        g->player.bazooka_rockets = 0;
        g->player.bazooka_firing = false;
        g->player.shot_vertical = 0;
        rocket->active = false;
        break;
    case 6:
    case 7:
        /*
         * On a rung, mid-throw, with the thing that is about to leave his hand
         * drawn in it — `draw_thrown_in_hand`, which no gate reached at all.
         *
         * It is the whole of the fix that gave it a reason to exist: the ladder
         * throw pose put a *grenade* in his hand for all three throwables, and
         * `draw_flashbang`'s own comment insists a charge "has to be told from a
         * grenade at a glance, because one of the two is about to kill whoever is
         * standing next to it". The simulation half of that fix got tests; the
         * drawing half — the half the argument is actually about — was executed
         * by nothing, because no headless run has ever *carried* a charge, and
         * `make coverage-shell` said so while the prose here claimed the only
         * things left were menu transitions.
         *
         * Two pages, because the function has an arm per throwable and the call
         * site has one branch per aim. Page 6 is the charge on a vertical aim,
         * page 7 the bolt thrown level — between them the flash arm, the decoy
         * arm and both sides of the `shot_vertical` branch. The grenade is the
         * fall-through of both, which is exactly why it was the only one ever
         * drawn.
         *
         * `grenade_throwing` is what `draw_player` reads to take this branch at
         * all, and it is shared by all three throwables on purpose — the pose is
         * one animation. `throwing_weapon` is the prop, and keeping the two
         * apart is the bug this stages the picture of.
         */
        g->player.on_ladder = true;
        g->player.active_weapon = page == 6 ? PLAYER_WEAPON_FLASH
                                            : PLAYER_WEAPON_DECOY;
        g->player.bazooka_rockets = 0;
        g->player.bazooka_firing = false;
        g->player.grenade_throwing = true;
        g->player.throwing_weapon = g->player.active_weapon;
        g->player.action_timer = PLAYER_KNIFE_ACTION_TIME;
        /* Carried, so the strip draws the row as well: the HUD reads the counts
         * rather than the pose, and a charge in the hand with nought on the
         * counter is a frame the game cannot reach. */
        g->player.flashbangs = page == 6 ? 1 : 0;
        g->player.shot_vertical = page == 6 ? -1 : 0;
        rocket->active = false;
        break;
    default:
        /* Standing and shooting sideways. */
        g->player.shot_vertical = 0;
        rocket->vx = 260.0f;
        rocket->vy = 0.0f;
        break;
    }

    /* Said out loud, because a floor with no `%` or no dog on it would stage
     * less than this screen claims to and the sweep would report it clean —
     * which is the failure `--screen` itself was written to end. */
    if (downed_men == 0 || downed_dogs == 0 || opened == 0)
    {
        SDL_Log("Sector %d cannot stage an aftermath: %d men, %d dogs, "
                "%d weak walls",
                g->level.map.mode == LEVEL_MODE_FACADE ? -1
                                                       : game->campaign.current_level + 1,
                downed_men, downed_dogs, opened);
        return false;
    }

    /*
     * And then the clock stops, which is the whole reason this is a still life.
     *
     * Entered at `STATE_PLAYING`, as it was first written, the simulation takes
     * its first step and clears every transient this function just wrote: nothing
     * is holding the crouch, so `crawling` goes false, and `action_timer` runs
     * out inside a tenth of a second. The bodies, the opened patch and the alarm
     * survived that and the four *poses* did not — a screen that staged them and
     * then drew somebody standing up. `STATE_PAUSED` is the one state whose own
     * comment is "time stands still", and it draws `render_world` underneath its
     * sheet, so the staged frame is the frame that gets rasterized.
     *
     * `pause_return_state` is set because `game_toggle_pause` is not what put us
     * here, and a sheet that resumes into nothing is a sheet holding a bad state
     * for the sanitizers to walk out through.
     */
    return soak_freeze_staged_frame(game);
}

GameScreenResult game_soak_screen(Game *game, const char *name, int page,
                                  int level_index)
{
    /*
     * The screens with a world behind them, through `--level`'s own entry point.
     *
     * This used to close "so nothing is banked and the record is not moved by a
     * sweep", and the last four words were carrying it: the *sweep* passes
     * `--soak` beside every one of these, and a scripted run has no files (see
     * `PlatformState.scripted`). A hand running `--screen cleared` had them, and
     * that card runs its timer down into `advance_level`, which banks — which is
     * where this developer's `furthest_sector 2` and `best_score 3230` came from,
     * a test gate's own figures sitting in a player's save. `--screen` makes the
     * run a scripted one now, so the sentence is true of the switch rather than
     * of one caller of it. A comment that is right about the common path reads as
     * a rule; this file has that lesson on it more than once.
     */
    static const char *const NEEDS_LEVEL[] = {"report", "cleared", "reveal",
                                              "pause", "continue", "gameover",
                                              "restroom", "aftermath"};
    bool needs_level = false;
    for (size_t i = 0; i < SDL_arraysize(NEEDS_LEVEL) && !needs_level; ++i)
        needs_level = SDL_strcmp(name, NEEDS_LEVEL[i]) == 0;

    if (needs_level)
    {
        /*
         * The sector `--level N` named, or sector 1 when it named none.
         *
         * It was sector 1 unconditionally, and for the restroom that was the
         * same defect `--page` was written to end: which room a `U` opens on is
         * decided by the sector's `THEME`, so pinning the entry point to the
         * lobby drew `restroom_lobby` and left the other three rooms compiled
         * under the sanitizers and never executed by them. Sector 1 is still
         * the default because it has a `U` and every card screen wants a world
         * behind it; naming a sector is what reaches the rest.
         */
        int wanted = level_index;
        if (wanted < 0)
        {
            /* Two screens whose default cannot be sector 1. The aftermath needs
             * a patch to open and a dog to put down, and the lobby has neither.
             * And the cleared card is only ever reached on the *last* sector —
             * `try_finish_current_level` sends every other floor to the report
             * or straight through a window — so staged on sector 1 it was drawn
             * over a world the state cannot occur in, and the run-down of its
             * timer then loaded sector 2 behind it. Every other card wants only
             * a world behind it. */
            if (SDL_strcmp(name, "aftermath") == 0)
                wanted = soak_aftermath_sector();
            else if (SDL_strcmp(name, "cleared") == 0)
                wanted = (int)EMBEDDED_LEVEL_COUNT - 1;
            else if (SDL_strcmp(name, "reveal") == 0)
                wanted = soak_reveal_sector();
            else
                wanted = 0;
        }
        if (!game_start_at_level(game, wanted))
            return GAME_SCREEN_REFUSED;
    }

    if (SDL_strcmp(name, "abduction") == 0)
        game_enter_state(game, STATE_ABDUCTION);
    else if (SDL_strcmp(name, "chase") == 0)
        game_enter_state(game, STATE_CHASE);
    else if (SDL_strcmp(name, "opening") == 0)
        game_enter_state(game, STATE_OPENING_CUTSCENE);
    else if (SDL_strcmp(name, "manual") == 0)
    {
        game_open_manual(game);
        /* Checked rather than assumed, because the open is now refused from any
         * state but the title screen and a paused run — so
         * `--screen manual --level 5` would otherwise soak a sector while the
         * sweep reported it had drawn a sheet. Naming a screen is not reaching
         * it; this file has that lesson written on it twice already. */
        if (game->state != STATE_MANUAL)
        {
            SDL_Log("Could not open the manual from this state");
            return GAME_SCREEN_REFUSED;
        }
        /* One screen name, ten drawings, and a headless run turns no sheet. See
         * `parse_screen_page` in main.c: without this the sweep covered
         * `illus_night` and left the other nine sanitizer-compiled and
         * sanitizer-unexecuted. */
        if (page > 0)
        {
            if (page > MANUAL_PAGE_COUNT)
            {
                SDL_Log("Sheet %d is outside the manual's %d", page,
                        MANUAL_PAGE_COUNT);
                return GAME_SCREEN_REFUSED;
            }
            /*
             * Turned to rather than jumped to, which costs nothing and covers
             * something: assigning `.page` reached the sheet and left
             * `manual_turn_page` — the only thing a reader ever uses to get
             * there, and the function that decides what a page turn *is* —
             * compiled under the sanitizers and never executed by them. Walking
             * is also how a hand reaches sheet six, so the sweep does what the
             * player does.
             */
            for (int turn = 1; turn < page; ++turn)
            {
                if (!manual_turn_page(&game->presentation.manual, 1))
                {
                    SDL_Log("The manual refused to turn to sheet %d", page);
                    return GAME_SCREEN_REFUSED;
                }
            }
        }
    }
    else if (SDL_strcmp(name, "settings") == 0)
    {
        game_open_settings(game);
        /*
         * `--page 2` is the controls page, which was unreachable by this sweep:
         * `draw_setting_keys` — every key cap and pad cap on it — was
         * sanitizer-compiled and never executed. The same argument as the
         * manual's `--page`, on the other sheet that is one screen name standing
         * for more than one drawing. It stood for two pages when that was
         * written and stands for `SETTINGS_PAGE_COUNT` of them, which is why the
         * bound below is the enum rather than a number.
         */
        if (page > 0)
        {
            if (page > SETTINGS_PAGE_COUNT)
            {
                SDL_Log("Page %d is outside the options sheet's %d", page,
                        (int)SETTINGS_PAGE_COUNT);
                return GAME_SCREEN_REFUSED;
            }
            settings_open_page(game, (SettingsPage)(page - 1));
        }
    }
    else if (SDL_strcmp(name, "outro") == 0)
        game_enter_state(game, STATE_OUTRO);
    else if (SDL_strcmp(name, "credits") == 0)
        game_enter_state(game, STATE_CREDITS);
    else if (SDL_strcmp(name, "resume") == 0)
    {
        /*
         * The title screen with a run to come back to, which is a *world state*
         * rather than a screen — the same kind of entry point as `aftermath`.
         *
         * It exists because the chip's coverage used to be an accident of whose
         * machine the sweep ran on. `intro.c` gates five things on
         * `resume_offered` — the chip's own width, the row's centring, the hit
         * plate and the drawing — and all of them come off
         * `progress.furthest_sector`, which was read from the runner's disk. A
         * developer who had played to sector 2 drew the chip every sweep; a
         * clean checkout drew it never, and neither of them could tell which
         * they were doing. `--shot`/`--soak` take the shipped defaults now (see
         * `PlatformState.scripted`), so without this the answer would have been
         * a *guaranteed* never, which is worse only in that it is quieter.
         *
         * A sector rather than the last one, because what the chip has to fit is
         * a two-digit number beside the START plate, and the widest of those is
         * what a full campaign leaves behind.
         */
        game->progress.furthest_sector = (int)EMBEDDED_LEVEL_COUNT - 1;
        game_enter_state(game, STATE_INTRO);
    }
    else if (SDL_strcmp(name, "report") == 0)
    {
        /*
         * Numbers a real clear could produce, including both bonuses and a
         * record it did not beat, so every branch of the field draws.
         *
         * The run has to be the *slower* of the two, and it was the quicker one
         * for as long as this staging existed: 74 against a best of 91 with
         * `best_is_new` false is a frame saying the player beat the record and
         * that the record stands, which is the one thing
         * `sector_tally_format`'s own comment forbids — it spells `NEW BEST`
         * rather than print a time the run has just replaced. Nothing failed,
         * because the code was right and only the staged pair was impossible;
         * what it cost is that `--shot` is where the press stills and the store
         * page come from, so the contradiction was in the pictures rather than
         * in the game. A counter cannot tell a frame that was drawn from a
         * frame anybody could read.
         */
        /*
         * **And it is the clear of whatever sector `--level` named**, which for
         * a release it was not: the two sector numbers were the literals `0`
         * and `1`, so every report this switch has ever drawn was sector one's,
         * whatever was asked for and whatever world was loaded behind it.
         *
         * What that cost is the table. The report is where six of
         * `TRANSITION_INTEL`'s sixteen rows are given a whole screen — the arc
         * the plot rests on — and five of the six had never been rasterized by
         * anything: the fit check measures how *wide* a row is and
         * `INTEL_ARC_SECTORS` pins which sectors reach one, and neither of
         * those puts a line on the glass. `--screen reveal`, one branch below,
         * was given exactly this treatment a release earlier for exactly this
         * reason, and its own comment closes "a placement is not covered
         * because its twin is". This is the twin.
         *
         * A sector that shows no report is refused rather than staged, because
         * every other correction to these screens has been toward a staged
         * frame being one the game can produce — the impossible clock pair
         * above, the reveal's off-by-one sector, the continue card's hearts.
         * Both halves of the question are read off the same two things
         * `try_finish_current_level` reads: is there a next sector, and does
         * this one leave by its window.
         */
        int completed = game->campaign.current_level;
        int next = completed + 1;
        if ((size_t)next >= EMBEDDED_LEVEL_COUNT)
        {
            SDL_Log("Sector %d is the last of the campaign, so nothing is "
                    "reported after it",
                    completed + 1);
            return GAME_SCREEN_REFUSED;
        }
        if (game->gameplay.level.map.has_window)
        {
            SDL_Log("Sector %d leaves by its window and shows no report; its "
                    "line rides the reveal of sector %d",
                    completed + 1, next + 1);
            return GAME_SCREEN_REFUSED;
        }
        int hostiles = sector_tally_soak_hostiles(
            level_authored_hostiles(&game->gameplay.level.map));
        level_transition_init(&game->presentation.level_transition,
                              completed, next,
                              SOAK_TALLY_ELAPSED, 2400, hostiles, 0,
                              campaign_time_bonus_for(SOAK_TALLY_ELAPSED),
                              SECTOR_CLEAN_BONUS, SOAK_TALLY_BEST, false,
                              sector_tally_soak_docket(completed + 1));
        game_enter_state(game, STATE_LEVEL_TRANSITION);
    }
    else if (SDL_strcmp(name, "cleared") == 0)
    {
        /*
         * The way out is open, because the only way anybody reaches this card
         * is by standing in it.
         *
         * `gameplay_player_reached_exit` answers the window first and then
         * refuses a door the run has no card for, and the last sector has no
         * `Y` — so a real clear of it has always had `exit_unlocked` set by the
         * time `try_finish_current_level` runs. Staged without it the strip
         * behind the card read a blinking red ACCESS LOCKED over SECTOR 17
         * CLEAR: a floor finished through a door nobody had opened. Same class
         * as the elapsed-and-best pair this screen's own fixture was corrected
         * for and as the full hearts beside `x0` on the two end cards — a
         * staged frame must be one the game can produce, and every assertion
         * this card has is a *width*, which a wrong state is exactly as wide
         * as a right one. Read off a capture; no gate on this side of the SDL
         * boundary can see it.
         */
        game->gameplay.level.runtime.exit_unlocked = true;
        /* With a tally pending, because the line under this card is the half
         * of it that only the last sector of a run ever reaches. Named after
         * the sector actually loaded rather than a hard-coded nought: this
         * screen is staged on the last floor now, and a card reading SECTOR 01
         * CLEAR over sector 17's HUD is a frame that contradicts itself. */
        sector_tally_set(&game->presentation.sector_tally,
                         game->campaign.current_level, SOAK_TALLY_ELAPSED,
                         SOAK_TALLY_BEST, false,
                         campaign_time_bonus_for(SOAK_TALLY_ELAPSED),
                         SECTOR_CLEAN_BONUS,
                         sector_tally_soak_docket(
                             game->campaign.current_level + 1),
                         intel_line(game->campaign.current_level));
        /* And the strip's own SCORE field, which the band contradicts at
         * nought — see `sector_tally_soak_score`. */
        game->campaign.score =
            sector_tally_soak_score(game->campaign.current_level + 1);
        game_enter_state(game, STATE_LEVEL_CLEARED);
    }
    else if (SDL_strcmp(name, "reveal") == 0)
    {
        /*
         * The sector coming up, with the tally riding over it — which is where
         * ten of `TRANSITION_INTEL`'s sixteen lines and eleven of the seventeen
         * clears' numbers actually land, and it was a frame no gate drew.
         *
         * `cleared` was the only staged screen that put a tally on the glass,
         * and it is the *other* placement: the card grows the band downward
         * under a verdict panel, this grows it upward off the bottom edge of
         * the frame with the map rising behind it. Verifying one and calling
         * the band checked is how the card came to print its story line through
         * `SHE IS TWENTY FEET AWAY` — a placement is not covered because its
         * twin is.
         *
         * Staged on whatever sector `--level` names, because which line is
         * carried is the sector's own — and on `soak_reveal_sector` when it
         * names none, which is the first boundary in the campaign that really
         * carries a band rather than sector one, where a report clears it.
         *
         * **The band belongs to the sector below the one being revealed**, and
         * for as long as this screen existed it did not. `sector_tally_set` was
         * handed `current_level`, which is the sector the reveal is *of*, so
         * every frame this screen has ever drawn said `SECTOR 10 CLEAR` over the
         * reveal of sector 10 with the HUD beside it also reading 10 — a pairing
         * the game cannot produce, since `load_level` has already stepped the
         * counter by the time this state is entered. It is the impossible
         * time-and-best pair the report's own fixture was corrected for, one
         * screen over: a staged frame is where the press stills and the store
         * page come from, and a counter cannot tell a frame that was drawn from a
         * frame anybody could read.
         *
         * Sector 1 gets no band at all, which is the truthful answer rather than
         * a special case: nothing has been cleared yet. It is not what the sweep
         * draws — `soak_reveal_sector` picks the first boundary that really
         * carries one — so a hand asking for `--level 1` is asking to see that.
         */
        if (game->campaign.current_level > 0)
            sector_tally_set(&game->presentation.sector_tally,
                             game->campaign.current_level - 1,
                             SOAK_TALLY_ELAPSED, SOAK_TALLY_BEST, false,
                             campaign_time_bonus_for(SOAK_TALLY_ELAPSED),
                             SECTOR_CLEAN_BONUS,
                             sector_tally_soak_docket(
                                 game->campaign.current_level),
                             intel_line(game->campaign.current_level - 1));
        /* Same field, same reason: an interior reveal draws the sector strip
         * behind this band, and a climb's does not — so leaving it at nought
         * was a frame that only happened to be honest on the sectors the sweep
         * stages. */
        if (game->campaign.current_level > 0)
            game->campaign.score =
                sector_tally_soak_score(game->campaign.current_level);
        game_enter_state(game, STATE_LEVEL_START);
    }
    else if (SDL_strcmp(name, "pause") == 0)
        game_toggle_pause(game);
    else if (SDL_strcmp(name, "continue") == 0)
    {
        /*
         * The lives have to be gone first, and forgetting that made this row of
         * the sweep draw the wrong screen for as long as it existed.
         *
         * `campaign_begin_continue` returns false while `lives > 0` and leaves
         * `continue_timer` at nought — the prompt is what a run with nothing
         * left is offered, so a fresh campaign is refused it. This entry point
         * ignored the refusal and forced `STATE_CONTINUE` anyway, so the first
         * `campaign_update_continue` read a spent timer and moved straight to
         * `STATE_GAME_OVER`: the sweep reported `screen continue ok` and
         * rasterized the game-over card twice, while `draw_continue_overlay`
         * stayed sanitizer-compiled and never sanitizer-executed. Naming a
         * screen is not the same as reaching it, which is this file's own
         * recurring defect committed by the switch written to end it.
         */
        game->campaign.lives = 0;
        /* And the hearts with them, because the strip is drawn over this card
         * and `gameplay_damage_player` is the only way anybody arrives here:
         * hp reaches nought and `finish_player_death` leaves it there. Staged
         * with the lives alone the frame read three full hearts beside `x0`,
         * which is a run that has everything to lose and nothing left to lose
         * it with — the impossible-pair defect the report's own fixture was
         * corrected for, on the card beside it. */
        game->gameplay.player.hp = 0;
        if (!campaign_begin_continue(&game->campaign))
        {
            SDL_Log("Could not offer the continue prompt");
            return GAME_SCREEN_REFUSED;
        }
        game_enter_state(game, STATE_CONTINUE);
    }
    else if (SDL_strcmp(name, "gameover") == 0)
    {
        /*
         * Staged exactly as the continue card above is, and for the same
         * reason — which is a sentence that had to be written twice because it
         * was only ever acted on once.
         *
         * The only way anybody reaches this card is the continue countdown
         * running out (`campaign_update_continue` in `update_scene`), so the
         * lives are gone and `finish_player_death` has left the hearts at
         * nought. Staged with neither, the strip drawn over `GAME OVER` read
         * three full hearts and `x3`: a run with everything still to lose, on
         * the card that says the run is finished. The prompt beside it was
         * corrected for exactly this, four lines up, in the same function — one
         * half of a symmetric defect, in the paragraph written to record the
         * other half.
         */
        game->campaign.lives = 0;
        game->gameplay.player.hp = 0;
        game_enter_state(game, STATE_GAME_OVER);
    }
    else if (SDL_strcmp(name, "restroom") == 0)
    {
        if (!enter_restroom(game))
        {
            SDL_Log("Could not open the restroom off sector %d",
                    game->campaign.current_level + 1);
            return GAME_SCREEN_REFUSED;
        }
    }
    else if (SDL_strcmp(name, "aftermath") == 0)
    {
        if (!soak_stage_aftermath(game, page))
            return GAME_SCREEN_REFUSED;
    }
    else if (SDL_strcmp(name, "cover") == 0)
    {
        /*
         * The press kit's key art: the title screen's night recomposed as a
         * poster — tower closer, the cordon on the street, Chuck on the wall
         * in the searchlights, no interface. It is a screen no run reaches,
         * on purpose: its one caller is tools/press_kit.sh, and the sweep
         * walks it so its drawing code is not the one renderer the
         * sanitizers never execute. The flag is set after the state, because
         * `game_enter_state` runs `intro_init` and that zeroes the struct.
         */
        game_enter_state(game, STATE_INTRO);
        game->presentation.intro.key_art = true;
    }
    else
    {
        SDL_Log("Unknown screen '%s'", name);
        return GAME_SCREEN_UNKNOWN;
    }
    return GAME_SCREEN_STAGED;
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
        /* A death costs the walk back, never the kit: everything Chuck was
         * carrying for the hard part survives it, and the sidearm is topped
         * back up as the consolation. What "everything" is is
         * `player_carry_loadout`'s to say rather than this comment's — it was
         * a list of two here, in prose and in code, and the third thing on it
         * was the flash charge, which is the one item in the game whose entire
         * subject is a floor having gone wrong. */
        Player fallen = game->gameplay.player;
        player_reset(&game->gameplay.player, &game->gameplay.level);
        player_carry_loadout(&game->gameplay.player, &fallen);
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
    /* Played on from here, so the frame is no longer a staged one. See
     * `PlatformState.staged_frame`. */
    game->platform.staged_frame = false;

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
    /* And nothing arrives armed. A sheet reopened with the warning still on it
     * would be one press from the thing the warning exists to prevent. */
    game->pause_abandon_armed = false;
    audio_play(&game->platform.audio, SFX_MENU_PAGE);
    game_enter_state(game, STATE_PAUSED);
}

/*
 * The window has gone out from under a sector somebody was playing.
 *
 * Nothing in this game handled a window event of any kind, and the world is
 * driven by `SDL_AppIterate` rather than by input — so alt-tabbing, switching
 * desktops or clicking on something else left the floor running: the patrols
 * walked, the alarm counted, the fans turned, and the man standing still in the
 * middle of it took whatever was coming. Measured with nothing pressed, over 64
 * seeds a sector: ten of the twelve interiors cost a heart inside thirty
 * seconds, the earliest at 3.83s, and sectors 6, 8 and 17 cost the whole life on
 * 64, 64 and 59 of the 64. Ten seconds of reading an email is a life on three
 * floors of twelve; only sectors 2 and 5 never touch a man who is not there.
 *
 * The argument is already written down four lines from the key that makes it:
 * ESC pauses "instead of being thrown away; an accidental ESC must never cost
 * the run". A window losing focus is the same accident with the hand nowhere
 * near the keyboard, and it was the one this game answered by playing on.
 *
 * Through `game_toggle_pause` rather than beside it, so the states in which
 * there is a run to protect are named once: that function is a no-op in every
 * state that is not one, which is what makes this three lines instead of a
 * second copy of the list.
 *
 * A **scripted** run is exempt, and that exemption is the whole reason this can
 * be a rule rather than a risk: `--shot`, `--soak` and `--screen` have nobody at
 * the keyboard, a pause sheet drawn over a capture is a picture of a menu, and a
 * soak that paused itself would spend its budget on a still frame. See
 * `GameRunKind`.
 *
 * Focus coming *back* deliberately does not resume. The player decides when they
 * are looking again; a sector that starts moving the instant the window lights
 * up is the same defect with a smaller window.
 */
void game_pause_on_focus_lost(Game *game)
{
    if (game == NULL || game->platform.scripted)
        return;
    if (game->state == STATE_PAUSED)
        return;
    game_toggle_pause(game);
}

void game_pause_move_cursor(Game *game, int delta)
{
    if (game->state != STATE_PAUSED || delta == 0)
        return;
    game->pause_cursor = (game->pause_cursor + delta + PAUSE_ITEM_COUNT) %
                         PAUSE_ITEM_COUNT;
    /* Walking away from the armed row disarms it, which is what the detail line
     * promises and is the same rule `settings_disarm_action_row` keeps. */
    game->pause_abandon_armed = false;
    audio_play(&game->platform.audio, SFX_MENU_PAGE);
}

/*
 * Put the cursor on ABANDON RUN and arm it, without taking it.
 *
 * This is what `Q` and the pad's `SELECT` do now. Both used to call
 * `game_return_to_intro` straight from the pause sheet — one press, no
 * confirmation, on a key that is also the default `BIND_WEAPON_NEXT` and a
 * button that sits beside START. See `PAUSE_ABANDON_ARMED`.
 *
 * A shortcut that *reaches* a decision is worth keeping; a second way of
 * *making* one that the sheet itself guards is not. So the shortcut lands on the
 * row, shows the warning, and the next press is answered by
 * `game_pause_activate` like any other.
 */
void game_pause_arm_abandon(Game *game)
{
    if (game->state != STATE_PAUSED)
        return;
    if (game->pause_cursor == PAUSE_ITEM_ABANDON && game->pause_abandon_armed)
    {
        /* Already standing on it and already warned: this is the second press,
         * and it means the same thing here as it does on the row. */
        game_return_to_intro(game);
        return;
    }
    game->pause_cursor = PAUSE_ITEM_ABANDON;
    game->pause_abandon_armed = true;
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
    case PAUSE_ITEM_MANUAL:
        game_open_manual(game);
        return;
    case PAUSE_ITEM_ABANDON:
        /* Armed on the first press and spent on the second, exactly as the
         * options sheet's RESET RECORDS row is — and this one is the more
         * expensive of the two, because a record can be set again and the run
         * being stood in cannot be resumed. */
        if (!game->pause_abandon_armed)
        {
            game->pause_abandon_armed = true;
            audio_play(&game->platform.audio, SFX_MENU_PAGE);
            return;
        }
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
static bool pref_file_path(const Game *game, char *out, size_t cap,
                           const char *name)
{
    /*
     * A script's run has no files, and this is the one place that has to know
     * it.
     *
     * Both loads below apply their defaults *before* asking for a path and both
     * saves return the moment they do not get one, so refusing here is the
     * whole of "the shipped defaults, and nobody's disk written" — six call
     * sites, no new branch at any of them. Putting it anywhere else would have
     * been six places to remember, which is how the leak got past a release in
     * the first place. See `PlatformState.scripted`.
     */
    if (game->platform.scripted)
        return false;

    char *dir = SDL_GetPrefPath(CHUCK_APP_ORG, CHUCK_APP_NAME);
    if (dir == NULL)
        return false;
    int written = SDL_snprintf(out, cap, "%s%s", dir, name);
    SDL_free(dir);
    return written > 0 && (size_t)written < cap;
}

static bool settings_file_path(const Game *game, char *out, size_t cap)
{
    return pref_file_path(game, out, cap, "settings.cfg");
}

/* Two files rather than one, because they answer different questions: the
 * settings are what the player decided, the progress is what happened. Wiping
 * a campaign must not cost somebody their volume levels. */
static bool progress_file_path(const Game *game, char *out, size_t cap)
{
    return pref_file_path(game, out, cap, "progress.cfg");
}

static void game_load_settings(Game *game)
{
    settings_defaults(&game->settings);

    char path[1024];
    if (!settings_file_path(game, path, sizeof(path)))
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
    if (!settings_file_path(game, path, sizeof(path)))
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
    if (!progress_file_path(game, path, sizeof(path)))
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
    if (!progress_file_path(game, path, sizeof(path)))
        return;

    /* Room for the two headline numbers and a line per tracked sector. It was
     * 256 while there were only the two, and a buffer sized for what the file
     * used to hold is how a feature that writes more of it silently stops
     * writing the end. */
    char text[2048];
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
    /*
     * An assisted run banks neither of them, and this is the one gate for both.
     *
     * See `CampaignState.assisted`: the switches take effect the instant they
     * are flipped, so a run that spent one sector on infinite lives is not a run
     * whose score belongs beside an unassisted one. The sector times are gated
     * at their own call site for the same reason, and `furthest_sector` is
     * deliberately not gated at all — the resume chip is navigation, not a
     * record.
     */
    if (!campaign_records_count(&game->campaign))
        return;

    /*
     * Two ratchets, one write. The docket is banked on exactly the same five
     * ways out as the score, and for the same reason: what a run came away with
     * is only decided once it has ended, and a sheet picked up on sector three
     * of a run that was then abandoned still happened.
     *
     * Written as `|` rather than `||` on purpose — short-circuiting would skip
     * the second ratchet whenever the first one moved, which is the frame it is
     * most likely to have moved too.
     */
    bool moved = progress_note_score(&game->progress, game->campaign.score);
    moved = progress_note_evidence(&game->progress,
                                   game->campaign.evidence_collected) ||
            moved;
    if (moved)
        game_save_progress(game);
}

void game_advance_render_clock(Game *game, float elapsed)
{
    /* Guarded against a step that is not a number so that one bad frame cannot
     * poison every animation in the game for the rest of the process: the five
     * readers feed this straight into `sinf`, and a NaN here would stay a NaN
     * for good. */
    if (game == NULL || !(elapsed > 0.0f))
        return;
    game->presentation.render_clock += elapsed;
}

int game_resume_sector(const Game *game)
{
    return game->progress.furthest_sector;
}

int game_best_evidence(const Game *game)
{
    return game->progress.best_evidence;
}

int game_best_score(const Game *game)
{
    return game->progress.best_score;
}

const float *game_sector_records(const Game *game)
{
    /* `PROGRESS_MAX_TRACKED_SECTORS` is deliberately larger than the campaign,
     * so a caller walking `CAMPAIGN_SECTORS` of these is inside the array by
     * construction — and the assertion is what keeps that true if either number
     * moves. */
    _Static_assert(CAMPAIGN_SECTORS <= PROGRESS_MAX_TRACKED_SECTORS,
                   "the campaign has to fit the records the file keeps");
    return game->progress.best_sector_time;
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

/*
 * Forget that an action row was pressed.
 *
 * Called from every input the sheet accepts other than a second press on the row
 * itself, which is what makes "anything else keeps them" true rather than a
 * hopeful sentence on a detail line. One function so that a new input on this
 * sheet has one thing to remember instead of a flag to find — and one field, so
 * that a second row that needs arming cannot be the one nobody remembered to
 * clear. It was `settings_disarm_records` and a `bool`, and the row next door
 * spent a release taking nine bindings away on one press.
 */
static void settings_disarm_action_row(Game *game)
{
    game->settings_armed_row = SETTING_NONE;
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
    settings_disarm_action_row(game);
    audio_play(&game->platform.audio, SFX_MENU_PAGE);
    game_enter_state(game, STATE_SETTINGS);
}

void game_close_settings(Game *game)
{
    if (game->state != STATE_SETTINGS)
        return;
    game->settings_capturing = false;
    settings_disarm_action_row(game);
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
    settings_disarm_action_row(game);
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
    settings_disarm_action_row(game);
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
    {
        /*
         * Armed on the first press and spent on the second, for every row that
         * cannot be undone rather than for the one somebody remembered. The
         * detail line under the row says which of the two presses the player is
         * looking at, so the sheet never asks for a confirmation it has not
         * shown, and `settings_row_armed_detail` deciding *which* rows do this
         * is what stopped `RESET CONTROLS` being the row that asked nothing.
         *
         * The gate is above the actions rather than inside each of them, which
         * is what makes it impossible to add a destructive row that skips it:
         * the only way past this `break` is to have been armed already.
         */
        if (settings_row_armed_detail(row->id) != NULL &&
            game->settings_armed_row != row->id)
        {
            game->settings_armed_row = row->id;
            audio_play(&game->platform.audio, SFX_MENU_PAGE);
            break;
        }

        SettingsPage opens = settings_row_opens(row->id);
        if (opens != SETTINGS_PAGE_COUNT)
        {
            settings_open_page(game, opens);
        }
        else if (row->id == SETTING_BINDINGS_RESET)
        {
            keybind_defaults(&game->settings.bindings);
            settings_disarm_action_row(game);
            audio_play(&game->platform.audio, SFX_CARD_TARGET);
        }
        else if (row->id == SETTING_RECORDS_RESET)
        {
            progress_clear_records(&game->progress);
            game_save_progress(game);
            settings_disarm_action_row(game);
            audio_play(&game->platform.audio, SFX_CARD_TARGET);
        }
        break;
    }
    case SETTING_ROW_SLIDER:
    case SETTING_ROW_TOGGLE:
        /* ENTER is a change input on these two, which is what lets a switch be
         * flipped without the hand leaving the row it is on. */
        game_settings_adjust(game, 1);
        break;
    case SETTING_ROW_HEADING:
    case SETTING_ROW_READOUT:
        /* Neither is a row the caret can be on — `settings_row_is_reachable`
         * refuses both — so these are here to keep the switch exhaustive rather
         * than because confirm can arrive on one. */
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
    settings_disarm_action_row(game);

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
    case SETTING_VETERAN:
        /*
         * The pace reaches the sector already running, exactly as the assist
         * switches do — a switch that only took effect at the next doorway
         * would be a setting the player cannot see having changed anything.
         *
         * **And so do the lives, which this comment used to deny.** It read
         * "the lives and the continues deliberately do *not*", on the argument
         * that both are handed out by `campaign_reset` and that reaching back
         * into a run to take two lives off somebody would be the one thing on
         * this sheet that costs a player something they already had — and it
         * finished by pointing at the row's own detail line as the proof. Every
         * clause of that was wrong except the first. `campaign_reset` is not the
         * only place lives are handed out; `campaign_accept_continue` is the
         * other, it asks `campaign->veteran`, and `apply_assist_to_state` has
         * kept that flag on this switch since the shipped bug where it did not.
         * So one flip does exactly the thing the comment called unthinkable, one
         * death later, and the line it cited as evidence was the second half of
         * the same mistake rather than a check on it.
         *
         * Only `continues_remaining` is genuinely next-run. The behaviour is
         * right and stays — `docs/screens.md` argues for it and
         * `test_the_veteran_run_is_three_numbers_and_no_more` requires it,
         * because a veteran continue handing back `PLAYER_LIVES` is the mode
         * lasting exactly one mistake. What changed is the row, which now says
         * THIS RUN TOO.
         *
         * The lesson is the one this tree keeps relearning: a comment that
         * quotes a user-facing string as its justification has made that string
         * part of the invariant, and nothing was holding either of them.
         */
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
    case SETTING_OPEN_DIFFICULTY:
    case SETTING_OPEN_RECORDS:
    case SETTING_BINDINGS_RESET:
    case SETTING_RECORDS_RESET:
    case SETTING_READOUT_FIRST:
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
        /*
         * And it is held open long enough to read whatever is written over it.
         *
         * The between-sectors line is drawn while this state is on screen and
         * nowhere else, and this state lasts exactly as long as the reveal — so
         * the line's time on the glass was `width * height / 3000` seconds,
         * 0.18s on the smallest sector and 0.43s on the tallest climb, for two
         * lines and about 120 characters. Which is to say the plot beat for ten
         * of the sixteen sector boundaries, and the `DOCKET n/12` that
         * [docs/story.md](../docs/story.md) put there so a player could learn
         * the count while there was still something to do about it.
         *
         * Held here rather than by letting the band ride on into play, and the
         * reason is worth keeping: during the reveal nobody is drawn yet, and
         * after it the player is standing at his spawn — which on the climbs and
         * on four of the five interiors that carry this line is the bottom row
         * of the map, directly behind a band pinned to the bottom edge. The
         * first attempt did exactly that and hid the climber for the whole hold.
         * A reveal is already the game saying "here is the floor"; nothing is
         * behind it to hide.
         *
         * The cost is 3.8s of non-interactive time on the ten window
         * transitions, against `LEVEL_TRANSITION_DURATION`'s 9.4s on the six
         * that show the report instead — so this is the cheaper of the two
         * beats, not a new expense.
         */
        if (game->presentation.sector_tally.pending)
            level_reveal_hold_for(&game->gameplay.level,
                                  SECTOR_TALLY_HOLD_TIME);
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
        game->presentation.message_timer = LEVEL_CLEARED_DISPLAY_TIME;
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
            game->presentation.outro_cutscene.time >= OUTRO_REPLAY_PROMPT_TIME)
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
     * five climbs and the roof would have been the six floors in the campaign
     * that paid nothing for being cleared quickly.
     */
    int time_bonus = 0;
    int clean_bonus = 0;
    campaign_award_sector_bonus(&game->campaign, &time_bonus, &clean_bonus);
    /*
     * The clock is banked before the report reads it, and the *old* record is
     * what the report is handed — otherwise a new best would be printed as the
     * thing it just beat, which is the field agreeing with itself and telling
     * the player nothing. `best_is_new` is what says which of the two happened.
     *
     * Banked on every way out of a sector for the reason the bonus above is:
     * the window onto a climb and the last floor of all are cleared sectors
     * too, and a record only the stair door could set would quietly exclude
     * ten of the seventeen.
     */
    float previous_best =
        progress_sector_time(&game->progress, game->campaign.current_level);
    /* Gated the same way the score is, and the report still prints the record
     * beside the stopwatch: an assisted run is measured against the ladder and
     * simply does not join it. See `CampaignState.assisted`. */
    bool sector_time_is_new_best =
        campaign_records_count(&game->campaign) &&
        progress_note_sector_time(&game->progress,
                                  game->campaign.current_level,
                                  game->campaign.level_elapsed_time);
    if (sector_time_is_new_best)
        game_save_progress(game);
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

    /*
     * The one line the report would have said, for the clears that never reach
     * one. See [sector_tally.h](sector_tally.h): a window and the last sector
     * of the campaign both skip the report, and both were still paying the
     * time bonus, paying the clean bonus and banking a per-sector record with
     * nothing on screen connecting the player to any of it.
     *
     * Set here rather than in the two branches below so the arguments are read
     * off the same locals the report is built from and cannot come to disagree
     * with it; the branch that *does* show a report clears it again, because a
     * screen that says all of this properly must not also be trailed by a
     * one-line summary of itself.
     *
     * **And the plot line rides with it**, off the same `intel_line` the report
     * reads. Rescuing the bonuses and leaving the sentence behind was this
     * file's own halfway fix: ten of the sixteen rows were written, measured,
     * pinned against the maps and shown to nobody. Same call, same locals, so
     * the two can no longer come apart — and NULL on the last sector, which has
     * no row.
     */
    sector_tally_set(&game->presentation.sector_tally,
                     game->campaign.current_level,
                     game->campaign.level_elapsed_time, previous_best,
                     sector_time_is_new_best, time_bonus, clean_bonus,
                     game->campaign.evidence_collected,
                     intel_line(game->campaign.current_level));

    if ((size_t)(game->campaign.current_level + 1) < EMBEDDED_LEVEL_COUNT)
    {
        int next_level = game->campaign.current_level + 1;
        if (game->gameplay.level.map.has_window)
        {
            /* A window is a continuous physical route between inside and the
             * facade. The hostage/elevator report belongs only between normal
             * interior sectors and would contradict what is on screen here.
             *
             * What the window does *not* excuse is the scoreboard going quiet
             * with it, which is what happened for as long as this branch
             * existed: the tally set above rides along and is drawn over the
             * next sector's reveal, which cuts away from nothing. */
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
            sector_tally_clear(&game->presentation.sector_tally);
            level_transition_init(
                &game->presentation.level_transition,
                game->campaign.current_level,
                next_level,
                game->campaign.level_elapsed_time,
                game->campaign.score - game->campaign.level_start_score,
                gameplay_neutralized_hostiles(&game->gameplay),
                game->campaign.level_deaths,
                time_bonus, clean_bonus,
                previous_best, sector_time_is_new_best,
                game->campaign.evidence_collected);
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

    /* After the walk, so a body follows the step Chuck has just taken rather
     * than the one before it, and after the terminal, which owns the same held
     * button and gets first claim on it. */
    gameplay_update_body_drag(&game->gameplay, &game->input);

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

    /* Before the perception pass below, not after it: a bolt that lands this
     * frame has to be a noise the guards get to hear this frame. Ordered the
     * other way it would be a frame late, which is invisible on its own and
     * exactly the kind of thing that makes a mechanic feel unreliable. */
    gameplay_combat_update_decoys(&game->gameplay, dt);

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
