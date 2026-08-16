#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "game.h"
#include "version.h"

/* `--level N` boots straight into campaign sector N (1-based), skipping the
 * title screen and the prologue. It is how the level editor playtests the map
 * being drawn; N is otherwise reached only by playing there. */
static int parse_start_level(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        if (SDL_strcmp(argv[i], "--level") != 0)
            continue;
        /* A switch with nothing after it is a typo, not a request for the title
         * screen. Every other bad input on this command line says so — a
         * sector outside the campaign, a scene nobody knows, a level number
         * below one — and silence here was the one case that let a mistyped
         * playtest look like a deliberate boot to the title. */
        if (i + 1 >= argc)
        {
            SDL_Log("--level needs a sector number after it");
            continue;
        }
        int level = SDL_atoi(argv[i + 1]);
        if (level >= 1)
            return level - 1;
        SDL_Log("--level expects a sector number of 1 or more");
    }
    return -1;
}

/* `--scene NAME` opens one screen directly. It is what lets `make smoke` reach
 * the presentation code that is otherwise only arrived at by playing; see
 * `game_start_at_scene`. Read after `--level`, so `--level 9 --scene report`
 * draws the report sector nine would have handed over. */
static const char *parse_start_scene(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        if (SDL_strcmp(argv[i], "--scene") != 0)
            continue;
        if (i + 1 >= argc)
        {
            /* Same rule as `--level` above. */
            SDL_Log("--scene needs a screen name after it");
            continue;
        }
        return argv[i + 1];
    }
    return NULL;
}

/* `--page N` opens the manual on one sheet (1-based, like `--level`). The book
 * is the one screen `--scene` cannot cover on its own: a sheet is only ever
 * turned by a hand, so a run that presses no keys draws the first of the eight
 * and none of the other seven. Read after `--scene`, and only the manual reads
 * it; see `game_show_manual_page`. */
static int parse_manual_page(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        if (SDL_strcmp(argv[i], "--page") != 0)
            continue;
        if (i + 1 >= argc)
        {
            /* Same rule as the two above. */
            SDL_Log("--page needs a sheet number after it");
            continue;
        }
        int page = SDL_atoi(argv[i + 1]);
        if (page >= 1)
            return page - 1;
        SDL_Log("--page expects a sheet number of 1 or more");
    }
    return -1;
}

/* `--demo` hands the sector to a scripted hand instead of a player's. It takes
 * no value, and it is read last because what it drives is whatever `--level`
 * and `--scene` have already opened. See `game_start_demo`. */
static bool parse_demo(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        if (SDL_strcmp(argv[i], "--demo") == 0)
            return true;
    }
    return false;
}

/*
 * Anything on the command line that is none of the switches above.
 *
 * The parsers each walk the whole line looking for their own switch and ignore
 * everything else, which is what let `./chuck --wat` and a misspelt
 * `--sector 9` boot the title screen without a word. A flag that does nothing
 * at all is the same bug as a prompt naming a button the state does not
 * accept — the player, or here the author, is told their input was accepted
 * when it was not. It is a note rather than a refusal, because the game itself
 * is perfectly able to run.
 */
static void warn_about_unknown_arguments(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        /* The one switch with no value after it, so it is stepped over on its
         * own rather than taking the next argument with it. */
        if (SDL_strcmp(argv[i], "--demo") == 0)
            continue;
        if (SDL_strcmp(argv[i], "--level") == 0 ||
            SDL_strcmp(argv[i], "--scene") == 0 ||
            SDL_strcmp(argv[i], "--page") == 0)
        {
            /* Step over the value the switch consumed, so `--scene report` does
             * not report `report` as an unknown argument of its own. */
            if (i + 1 < argc)
                ++i;
            continue;
        }
        SDL_Log("Ignoring unknown argument '%s'; this build knows "
                "--level N, --scene NAME, --page N and --demo",
                argv[i]);
    }
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    /* Before SDL_Init, because this is what SDL names the process with: without
     * it the audio device, the window's owner and macOS's own crash reports all
     * say "SDL Application". It is the same identity the .app bundle carries,
     * out of the one header that holds it. */
    SDL_SetAppMetadata(CHUCK_APP_NAME, CHUCK_VERSION, CHUCK_APP_ID);

    Game *game = (Game *)SDL_calloc(1, sizeof(Game));
    if (game == NULL)
    {
        return SDL_APP_FAILURE;
    }

    if (!game_init(game))
    {
        SDL_free(game);
        return SDL_APP_FAILURE;
    }

    warn_about_unknown_arguments(argc, argv);

    int start_level = parse_start_level(argc, argv);
    if (start_level >= 0)
    {
        game_start_at_level(game, start_level);
    }

    /* After the level, because the screens that report on a sector are drawn
     * from whatever `--level` has just loaded. */
    const char *start_scene = parse_start_scene(argc, argv);
    if (start_scene != NULL)
    {
        game_start_at_scene(game, start_scene);
    }

    /* And after the scene, because the sheet it names is a sheet of whatever
     * the switch above has just opened. */
    int manual_page = parse_manual_page(argc, argv);
    if (manual_page >= 0)
    {
        game_show_manual_page(game, manual_page);
    }

    /* Last of the four, because it drives whatever the three above have
     * opened. */
    if (parse_demo(argc, argv))
    {
        game_start_demo(game);
    }

    *appstate = game;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    Game *game = (Game *)appstate;

    if (event->type == SDL_EVENT_QUIT)
    {
        return SDL_APP_SUCCESS;
    }
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE && !event->key.repeat)
    {
        if (game->state == STATE_INTRO)
        {
            return SDL_APP_SUCCESS;
        }
        /* Anything actually running pauses instead of being thrown away; an
         * accidental ESC must never cost the run. Quitting to the title is a
         * deliberate second step from the pause screen. */
        if (game->state == STATE_PLAYING || game->state == STATE_LEVEL_START ||
            game->state == STATE_SHOW_KEYCARD || game->state == STATE_CHASE ||
            game->state == STATE_PAUSED)
        {
            game_toggle_pause(game);
            return SDL_APP_CONTINUE;
        }
        if (game->state == STATE_SETTINGS)
        {
            /* Innermost first, the same order the sheet's own back key uses:
             * an armed capture, then the controls page, then the sheet. ESC is
             * deliberately not a bindable key precisely so that it can always
             * be the way out of a capture. */
            if (!game_settings_leave_page(game))
                game_close_settings(game);
            return SDL_APP_CONTINUE;
        }
        /* The report between sectors is mid-campaign and has nothing to close,
         * so ESC does nothing there rather than dropping a run in progress on
         * the title screen — the same rule the pad's B follows. The continue
         * prompt is on the list for the same reason: it is a live decision with
         * a run still on the table, and one stray ESC answering it "no" is the
         * accident this whole branch exists to prevent. Letting the countdown
         * expire already reaches the title, and does it in the player's time.
         *
         * STATE_LEVEL_CLEARED is the beat between finishing the last sector and
         * the outro starting. It is barely a second long, it is the one moment
         * in the game where the player has just won, and ESC landing in it
         * replaced the ending with the title screen — the single most expensive
         * thing a stray keypress could take, at the single worst moment. */
        if (game->state == STATE_LEVEL_TRANSITION ||
            game->state == STATE_LEVEL_CLEARED ||
            game->state == STATE_CONTINUE)
        {
            return SDL_APP_CONTINUE;
        }
        /* What is left is the prologue's two cutscenes, the manual, the outro,
         * the roll of names after it and the game-over hold: nothing with a run
         * still on the table, so here ESC is the way out rather than an
         * accident. The credits reach the title screen on their own anyway;
         * ESC only says so sooner. */
        game_return_to_intro(game);
        return SDL_APP_CONTINUE;
    }

    game_handle_event(game, event);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    Game *game = (Game *)appstate;

    Uint64 now = SDL_GetTicksNS();
    float elapsed = (float)(now - game->platform.last_tick) / 1.0e9f;
    game->platform.last_tick = now;
    /* Nothing downstream ever sees a longer step: MAX_FRAME_DT is what the
     * projectile collision is proved against, not just a stutter guard. It is
     * also what bounds the loop below — a frame this long can queue at most
     * SIM_STEPS_PER_SECOND / MIN_FRAME_RATE steps, so a machine that cannot
     * keep up runs the world slow instead of spiralling trying to catch it. */
    if (elapsed > MAX_FRAME_DT)
    {
        elapsed = MAX_FRAME_DT;
    }

    /*
     * Real time in, fixed steps out. The simulation is advanced in whole
     * SIM_STEP_DT slices and whatever is left over waits here for the next
     * frame, so what the physics produces is a property of the game rather
     * than of the display it happens to be drawn on. See SIM_STEP_DT.
     */
    game->platform.sim_accumulator += elapsed;
    while (game->platform.sim_accumulator >= SIM_STEP_DT)
    {
        game->platform.sim_accumulator -= SIM_STEP_DT;
        game_update(game, SIM_STEP_DT);
    }

    game_render(game);

    /* The title screen's quit chip, answered here because that is where an
     * SDL_AppResult can still be returned. ESC on the same screen goes out
     * through SDL_AppEvent above and never sets this. */
    if (game->quit_requested)
    {
        return SDL_APP_SUCCESS;
    }

    /* And the frame floor, for the machines where the swap above did not wait
     * for anything. `frame_min_ns` is nought whenever vsync was granted, which
     * is the ordinary case and costs a compare. */
    if (game->platform.frame_min_ns != 0)
    {
        Uint64 spent = SDL_GetTicksNS() - now;
        if (spent < game->platform.frame_min_ns)
            SDL_DelayNS(game->platform.frame_min_ns - spent);
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    Game *game = (Game *)appstate;
    if (game != NULL)
    {
        game_shutdown(game);
        SDL_free(game);
    }
}
