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
         * sector outside the campaign, a level number below one — and silence
         * here was the one case that let a mistyped playtest look like a
         * deliberate boot to the title. */
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

/*
 * `--soak N` closes the window by itself after N seconds.
 *
 * It is not a play mode and nothing in the game reads it. Its one caller is
 * [../tools/soak.sh](../tools/soak.sh), which walks the sanitized build across
 * every sector so that ASan and UBSan reach the renderers, the level art and
 * the audio synth — see `PlatformState.soaking` for why that needed a switch of
 * its own rather than a `kill` from the script.
 *
 * Nought and below are refused for the reason `--level 0` is: a soak of no
 * seconds is a typo, and honouring it would exit before the first frame and
 * report a pass for a build nothing had drawn.
 */
static float parse_soak_seconds(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        if (SDL_strcmp(argv[i], "--soak") != 0)
            continue;
        if (i + 1 >= argc)
        {
            SDL_Log("--soak needs a number of seconds after it");
            continue;
        }
        double seconds = SDL_atof(argv[i + 1]);
        if (seconds > 0.0)
            return (float)seconds;
        SDL_Log("--soak expects a positive number of seconds");
    }
    return 0.0f;
}

/*
 * `--screen NAME` opens one named screen and stays on it.
 *
 * The soak's own switch, exactly as `--soak` is, and it exists for the same
 * reason: the sweep was reporting coverage of screens no headless run had ever
 * drawn. See `game_soak_screen` for the list and for why this is a switch
 * rather than synthesised keypresses.
 */
static const char *parse_screen(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        if (SDL_strcmp(argv[i], "--screen") != 0)
            continue;
        if (i + 1 >= argc)
        {
            SDL_Log("--screen needs a screen name after it");
            continue;
        }
        return argv[i + 1];
    }
    return NULL;
}

/*
 * `--page N` picks which sheet `--screen manual` opens on, 1-based — and which
 * half of `--screen settings`, and which of `--screen aftermath`'s four poses.
 *
 * The manual is ten sheets behind one screen name, and nothing turns a sheet but
 * a hand: a headless run receives no events, so the sweep drew sheet one and the
 * nine illustrations behind it were compiled under the sanitizers and never
 * executed by them. That is the same defect `--screen` itself was written for,
 * one level further in — and the sheaf is where it costs most, since the
 * illustrations are some six hundred lines of drawing that no test reaches.
 *
 * Nought and below are refused the way `--level 0` is: a page number that is not
 * a page is a typo, and honouring it by opening sheet one would report coverage
 * of a sheet nobody asked for.
 */
static int parse_screen_page(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        if (SDL_strcmp(argv[i], "--page") != 0)
            continue;
        if (i + 1 >= argc)
        {
            SDL_Log("--page needs a sheet number after it");
            continue;
        }
        int page = SDL_atoi(argv[i + 1]);
        if (page >= 1)
            return page;
        SDL_Log("--page expects a sheet number of 1 or more");
    }
    return 0;
}

/*
 * Anything on the command line that is not one of the switches above.
 *
 * The parser walks the whole line looking for its own switch and ignores
 * everything else, which is what let `./chuck --wat` and a misspelt
 * `--sector 9` boot the title screen without a word. A flag that does nothing
 * at all is the same bug as a prompt naming a button the state does not
 * accept — the player, or here the author, is told their input was accepted
 * when it was not. It is a note rather than a refusal, because the game itself
 * is perfectly able to run.
 *
 * The switches are listed from one table rather than as a branch each, because
 * that is the copy that goes stale: one added to the parsers above and
 * forgotten here is a flag the game accepts and this function reports as
 * unknown, and the message it prints would still name only the older ones.
 * `--screen` is the third and arrived exactly that way.
 */
static void warn_about_unknown_arguments(int argc, char *argv[])
{
    /* Every switch that takes a value after it, which is all of them so far. */
    static const char *const SWITCHES[] = {"--level", "--soak", "--screen",
                                          "--page"};
    const int switch_count = (int)(sizeof(SWITCHES) / sizeof(SWITCHES[0]));

    for (int i = 1; i < argc; ++i)
    {
        bool known = false;
        for (int s = 0; s < switch_count && !known; ++s)
        {
            if (SDL_strcmp(argv[i], SWITCHES[s]) != 0)
                continue;
            known = true;
            /* Step over the value the switch consumed, so `--level 9` does not
             * report `9` as an unknown argument of its own. */
            if (i + 1 < argc)
                ++i;
        }
        if (known)
            continue;
        SDL_Log("Ignoring unknown argument '%s'; this build knows "
                "--level N, --soak SECONDS and --screen NAME",
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

    /* Handed over before anything below can refuse the command line, because
     * SDL calls `SDL_AppQuit` on an init that fails and hands it whatever this
     * has been set to. Left until the end, a bad `--screen` walked out through
     * a teardown holding NULL and the whole `Game` was still allocated — a
     * leak on the one path the sanitizers exist to walk. */
    *appstate = game;

    warn_about_unknown_arguments(argc, argv);

    int start_level = parse_start_level(argc, argv);
    if (start_level >= 0)
    {
        game_start_at_level(game, start_level);
    }

    /* After `--level`, because a screen that needs a sector behind it loads its
     * own and must be the last word on where the game ends up — and the sector
     * it loads is now the one `--level` named, which is what lets
     * `--screen restroom --level 5` reach a room other than the lobby's. A
     * negative `start_level` is "none given" and leaves the choice to the
     * screen. */
    const char *screen = parse_screen(argc, argv);
    if (screen != NULL &&
        !game_soak_screen(game, screen, parse_screen_page(argc, argv),
                          start_level))
    {
        SDL_Log("--screen expects one of: abduction, chase, opening, manual, "
                "settings, pause, report, cleared, continue, gameover, outro, "
                "credits, restroom, aftermath");
        return SDL_APP_FAILURE;
    }

    float soak = parse_soak_seconds(argc, argv);
    if (soak > 0.0f)
    {
        game->platform.soaking = true;
        game->platform.soak_seconds_left = soak;
        /* Said out loud, because a soak that quits on its own and a soak that
         * crashed on its own look identical in a log otherwise. */
        SDL_Log("Soaking for %.1f seconds, then closing", (double)soak);
    }

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

    /*
     * The soak budget, spent before the clamp below rather than after it. See
     * `PlatformState.soaking`: a sanitized frame can outlast MAX_FRAME_DT, and
     * a budget paid in clamped time would turn `--soak 2` into two minutes.
     *
     * Returning SDL_APP_SUCCESS is what makes this worth a switch at all — the
     * process walks out through `SDL_AppQuit` and `game_shutdown`, so the
     * teardown is sanitized too. A killed process never reaches either.
     */
    if (game->platform.soaking)
    {
        game->platform.soak_seconds_left -= elapsed;
        if (game->platform.soak_seconds_left <= 0.0f)
        {
            SDL_Log("Soak finished; closing");
            return SDL_APP_SUCCESS;
        }
    }
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
