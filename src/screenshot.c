#include "screenshot.h"

#include "game_config.h"

/* The numbered name a burst's frame is written under. A single frame keeps the
 * name it was given; a burst numbers from one, before the extension rather than
 * after it, so the files sort in order and still open as pictures. */
static void shot_frame_path(const ShotPlan *plan, char *out, size_t size,
                            int index)
{
    if (plan->frames <= 1)
    {
        SDL_strlcpy(out, plan->path, size);
        return;
    }

    /* The extension is the last dot in the last path component. A dot earlier in
     * the path is a directory's, and a path with no dot at all is a name without
     * an extension, which is legal — both end up numbered at the tail. */
    const char *extension = SDL_strrchr(plan->path, '.');
    const char *separator = SDL_strrchr(plan->path, '/');
    if (extension == NULL || (separator != NULL && extension < separator))
    {
        extension = plan->path + SDL_strlen(plan->path);
    }

    size_t stem = (size_t)(extension - plan->path);
    if (stem >= size)
    {
        stem = size - 1;
    }
    SDL_memcpy(out, plan->path, stem);
    out[stem] = '\0';

    char number[16];
    SDL_snprintf(number, sizeof(number), "-%03d", index + 1);
    SDL_strlcat(out, number, size);
    SDL_strlcat(out, extension, size);
}

bool shot_plan_open(ShotPlan *plan, const char *path, int frames, float rate,
                    float lead_in)
{
    SDL_zerop(plan);

    if (path == NULL || path[0] == '\0')
    {
        SDL_Log("--shot needs a file to write");
        return false;
    }
    if (frames < 1)
    {
        SDL_Log("--shot-frames expects a count of 1 or more");
        return false;
    }
    /* Below MIN_FRAME_RATE the frame loop clamps the step it is handed, so a
     * capture asked for at fifteen a second would be drawn at fifteen and
     * simulated at twenty — a GIF that plays back slower than the game. A
     * refusal rather than a clamp, because that is the kind of wrongness nobody
     * would notice. */
    if (!(rate >= (float)MIN_FRAME_RATE))
    {
        SDL_Log("--shot-fps expects %d or more, because a longer step than "
                "MAX_FRAME_DT is clamped by the frame loop",
                MIN_FRAME_RATE);
        return false;
    }
    if (lead_in < 0.0f)
    {
        SDL_Log("--shot-at expects a number of seconds of nought or more");
        return false;
    }

    plan->path = path;
    plan->frames = frames;
    plan->rate = rate;
    plan->lead_seconds_left = lead_in;
    return true;
}

bool shot_plan_active(const ShotPlan *plan)
{
    return plan->path != NULL && plan->written < plan->frames;
}

float shot_plan_step_dt(const ShotPlan *plan)
{
    return shot_plan_active(plan) ? 1.0f / plan->rate : 0.0f;
}

void shot_plan_advance(ShotPlan *plan, float dt)
{
    if (!shot_plan_active(plan))
    {
        return;
    }
    if (plan->lead_seconds_left > 0.0f)
    {
        plan->lead_seconds_left -= dt;
    }
    plan->due = plan->lead_seconds_left <= 0.0f;
}

void shot_plan_capture(ShotPlan *plan, SDL_Renderer *renderer)
{
    if (!plan->due)
    {
        return;
    }
    plan->due = false;

    char path[1024];
    shot_frame_path(plan, path, sizeof(path), plan->written);
    if (screenshot_write(renderer, path))
    {
        ++plan->written;
        return;
    }

    /* One unwritable frame ends the plan rather than being retried: nothing about
     * a missing directory or a full disk improves on the next frame, and a burst
     * that limps on becomes a short GIF nobody questions. */
    plan->failed = true;
    plan->written = plan->frames;
}

bool shot_plan_complete(const ShotPlan *plan)
{
    return plan->path != NULL && plan->written >= plan->frames;
}

bool shot_plan_broke(const ShotPlan *plan)
{
    return plan->failed;
}

bool screenshot_write(SDL_Renderer *renderer, const char *path)
{
    /* NULL is the whole render target, which under letterbox presentation is the
     * window rather than the logical frame — so a resized window captures its bars
     * too. The press script uses the window the game opens, which is exact. */
    SDL_Surface *frame = SDL_RenderReadPixels(renderer, NULL);
    if (frame == NULL)
    {
        SDL_Log("--shot could not read the frame back: %s", SDL_GetError());
        return false;
    }

    bool ok = SDL_SaveBMP(frame, path);
    if (!ok)
    {
        SDL_Log("--shot could not write %s: %s", path, SDL_GetError());
    }
    SDL_DestroySurface(frame);
    return ok;
}
