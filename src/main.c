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
        if (SDL_strcmp(argv[i], "--level") != 0 || i + 1 >= argc)
            continue;
        int level = SDL_atoi(argv[i + 1]);
        if (level >= 1)
            return level - 1;
        SDL_Log("--level expects a sector number of 1 or more");
    }
    return -1;
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

    int start_level = parse_start_level(argc, argv);
    if (start_level >= 0)
    {
        game_start_at_level(game, start_level);
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
    float dt = (float)(now - game->platform.last_tick) / 1.0e9f;
    game->platform.last_tick = now;
    /* Nothing downstream ever sees a longer step: MAX_FRAME_DT is what the
     * projectile collision is proved against, not just a stutter guard. */
    if (dt > MAX_FRAME_DT)
    {
        dt = MAX_FRAME_DT;
    }

    game_update(game, dt);
    game_render(game);

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
