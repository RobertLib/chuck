#ifndef CHUCK_SCREENSHOT_H
#define CHUCK_SCREENSHOT_H

#include <SDL3/SDL.h>
#include <stdbool.h>

/*
 * Writing the frame the game just drew to a file.
 *
 * All of the art in Chuck is drawn at runtime, so there is no image anywhere in
 * the tree to put on a store page, in the README or in a bug report: the only
 * place it exists is the back buffer. `tools/press_kit.sh` is the caller.
 *
 * The clock is synthetic, which is the one design decision here. A capture
 * advances the world by exactly `1 / rate` seconds per drawn frame rather than by
 * whatever the machine managed, so a burst is evenly spaced by construction and
 * plays back at the rate it was asked for — which is what a GIF is. A capture
 * therefore spends *simulated* seconds, where `--soak` spends real ones: that one
 * is testing a process that closes itself before a script gives up on it, and
 * this one is photographing a moment in the game.
 */

/* One frame, and a second of lead-in before it. The lead-in is not decoration:
 * most screens here arrive on a fade or a slide, and their first frame is a
 * picture of nothing much. */
#define SHOT_FRAMES_DEFAULT 1
#define SHOT_RATE_DEFAULT 30.0f
#define SHOT_LEAD_IN_DEFAULT 1.0f

/*
 * A capture in progress: what was asked for, and how much of it has been written.
 *
 * The two halves of it are at opposite ends of a frame — `SDL_AppIterate` owns
 * the clock, `game_render` owns the pixels — so it lives in `PlatformState`
 * beside the soak budget, which is the same kind of request.
 */
typedef struct
{
    /* NULL unless `--shot` named one. Pointed straight at `argv`, which
     * outlives every frame this is read on. */
    const char *path;
    int frames;
    float rate;
    float lead_seconds_left;
    int written;
    bool due;
    bool failed;
} ShotPlan;

/* Take the request off the command line, or refuse it with a reason: a capture
 * that quietly produced nothing would leave a stale picture on a store page. */
bool shot_plan_open(ShotPlan *plan, const char *path, int frames, float rate,
                    float lead_in);

/* Whether frames are still owed. */
bool shot_plan_active(const ShotPlan *plan);

/* The synthetic step to run the frame loop at, or nought when nothing is being
 * captured and the loop should spend the real elapsed time as usual. */
float shot_plan_step_dt(const ShotPlan *plan);

/* Spend `dt` of the lead-in, and decide whether the frame about to be drawn is
 * one of the ones asked for. Once the lead-in is paid every frame is, which is
 * what makes a burst consecutive rather than sampled. */
void shot_plan_advance(ShotPlan *plan, float dt);

/* Write the frame if this one was asked for. Called from `game_render` with the
 * finished frame still on the back buffer: a presented frame is gone, and
 * reading it after the swap is reading whatever the driver left behind. */
void shot_plan_capture(ShotPlan *plan, SDL_Renderer *renderer);

/* Whether the plan has run out — either every frame was written, or one of them
 * could not be. `shot_plan_broke` is the difference between those two, and it is
 * what makes a failed capture a failed process rather than a quiet one. */
bool shot_plan_complete(const ShotPlan *plan);
bool shot_plan_broke(const ShotPlan *plan);

/* The frame on the back buffer, written to `path` as a BMP — because SDL writes
 * one with nothing linked against it, and the press script has ImageMagick to
 * hand. A PNG writer here would be a compression library in a game that ships no
 * asset files. */
bool screenshot_write(SDL_Renderer *renderer, const char *path);

#endif /* CHUCK_SCREENSHOT_H */
