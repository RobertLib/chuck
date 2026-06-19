#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "game.h"
#include "screenshot.h"
#include "version.h"

/*
 * The number after a switch, read strictly, once, for all of them.
 *
 * **The strict parse was written for the capture switches and the two older
 * parsers were left on `SDL_atof` and `SDL_atoi`**, which is how `--soak inf`
 * came to log `Soaking for inf seconds, then closing` and then never close:
 * `inf` is a thing `SDL_atof` reads happily, it is greater than nought, so it
 * passed the range check and became a budget that counts down forever. No
 * capture, no error, no exit. That is the failure every workflow under
 * `.github` spends its `timeout-minutes` on — a step that neither finishes nor
 * fails, reporting the clock instead of the cause — produced by the one switch
 * in this binary whose *entire* justification is that a script rather than a
 * hand drives it. `--soak 1e400` is the same hole reached by arithmetic rather
 * than by spelling, and `--soak 3s` and `--page 3s` were quietly read as 3.
 *
 * The comment on `parse_shot_number` below had already argued every line of
 * this — "`SDL_atof` cannot fail", the `nan` that "never closed", and that it
 * "is the same sentence this file already carries about `--soak`" — while
 * `--soak` went on calling `SDL_atof` eighty lines above it. **A rationale is
 * not a check**, and a fix that lands on one of three copies of a rule is the
 * most reliable way to stop anybody looking at the other two.
 *
 * So the strictness is written down once and the *policy* stays at the call
 * sites, where it already is: what range a number has to fall in, and what a
 * refusal costs the process. Those two differ per switch on purpose — see
 * `SOAK_MALFORMED` for why a script gets an exit code and a hand gets a
 * running game — and neither is a question about whether a token is a number.
 *
 * The noun in "needs ... after it" is the call site's for the same reason: the
 * three messages this replaced each named what they wanted, and a shared reader
 * is not a licence to make them vaguer.
 *
 * `parse_seed` deliberately does not come through here, and the reason is
 * precision rather than taste: a seed is any 64-bit value and a `double` stops
 * being able to hold one at 2^53, so it reads through `SDL_strtoull`. It has
 * been strict since it was written, for the reason written beside it.
 */
typedef enum
{
    /* The switch was not on the command line. */
    SWITCH_NUMBER_ABSENT = 0,
    /* A number was read, and the whole token was a number. */
    SWITCH_NUMBER_READ,
    /* Asked for and unusable. The refusal is already logged. */
    SWITCH_NUMBER_BAD
} SwitchNumber;

static SwitchNumber parse_switch_number(int argc, char *argv[],
                                        const char *name, const char *noun,
                                        double *value, const char **text)
{
    for (int i = 1; i < argc; ++i)
    {
        if (SDL_strcmp(argv[i], name) != 0)
            continue;
        if (i + 1 >= argc)
        {
            /* The noun is the caller's, because "needs a number after it" is
             * vaguer than the three messages this replaced — a sector number, a
             * number of seconds and a sheet number — and a message a script or
             * an author acts on is not a place to save a parameter. The two
             * refusals below quote the offending token instead, which is the
             * thing to act on there. */
            SDL_Log("%s needs %s after it", name, noun);
            return SWITCH_NUMBER_BAD;
        }
        const char *token = argv[i + 1];
        /* Something has to have been read, and all of it. `SDL_strtod` stops at
         * the first character it cannot use, so a bare check on the value
         * accepts `3s` and `--soak 3 --screen` alike. */
        char *end = NULL;
        double read = SDL_strtod(token, &end);
        if (end == token || *end != '\0')
        {
            SDL_Log("%s expects a number, not '%s'", name, token);
            return SWITCH_NUMBER_BAD;
        }
        /* Refused here rather than downstream, because downstream is a range
         * check and every range check in this binary is a `<` comparison — all
         * of which are false against a NaN, and none of which has an upper
         * bound an infinity falls outside. */
        if (SDL_isnan(read) || SDL_isinf(read))
        {
            SDL_Log("%s expects a real number, not '%s'", name, token);
            return SWITCH_NUMBER_BAD;
        }
        *value = read;
        if (text != NULL)
            *text = token;
        return SWITCH_NUMBER_READ;
    }
    return SWITCH_NUMBER_ABSENT;
}

/* Whether a number read off the command line is a whole one that fits an `int`,
 * which is what the two switches naming a 1-based index need of it. Asked as a
 * question about the value rather than by re-reading the token, so that `3.5`
 * and `3s` are refused by two different sentences: one is not a sheet, the
 * other is not a number. */
static bool switch_number_is_index(double value)
{
    return value >= 1.0 && value <= (double)SDL_MAX_SINT32 &&
           value == SDL_floor(value);
}

/* `--level N` boots straight into campaign sector N (1-based), skipping the
 * title screen and the prologue. It is how the level editor playtests the map
 * being drawn; N is otherwise reached only by playing there. */
static int parse_start_level(int argc, char *argv[])
{
    /* A switch with nothing after it is a typo, not a request for the title
     * screen; so is a sector that is not a whole number of one or more. Every
     * bad input on this command line says so, and the *only* thing this switch
     * does differently is that saying so does not stop the process — see
     * `SOAK_MALFORMED` for the argument, and note that it is an argument about
     * the exit code rather than about what counts as a sector number. This used
     * to read through `SDL_atoi`, so `--level 3s` was sector 3 and said
     * nothing. */
    double level = 0.0;
    const char *text = NULL;
    switch (parse_switch_number(argc, argv, "--level", "a sector number", &level,
                                &text))
    {
    case SWITCH_NUMBER_ABSENT:
    case SWITCH_NUMBER_BAD:
        return -1;
    case SWITCH_NUMBER_READ:
        break;
    }
    if (switch_number_is_index(level))
        return (int)level - 1;
    SDL_Log("--level expects a sector number of 1 or more, not '%s'", text);
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
 *
 * **And a refusal here has to stop the process, which it did not.** Three
 * returns, not two: positive is a budget, nought is "not asked", and
 * `SOAK_MALFORMED` is "asked for and unusable". It used to answer nought to both
 * of the last two, so `--soak abc` printed the line below and then opened the
 * title screen and sat there — forever, because a headless process receives no
 * events and nothing was ever going to close it. That is the one thing this
 * switch exists to make impossible: it is the only switch in the game that a
 * *script* rather than a player uses, and the failure it turns into is the one
 * every workflow under `.github` spends its `timeout-minutes` on — a step that
 * neither finishes nor fails, reporting the clock instead of the cause. The
 * message was never the problem; the exit status was.
 *
 * `--level` deliberately keeps the older shape and falls through to the title
 * screen, and the asymmetry is the point rather than an oversight: that switch is
 * for a person, `chuck-editor`'s playtest button is its caller, and a typo there
 * should leave somebody in the game rather than at a shell prompt. A switch a
 * script drives owes the script an exit code; a switch a hand drives owes the
 * hand a running game.
 */
#define SOAK_MALFORMED (-1.0f)

static float parse_soak_seconds(int argc, char *argv[])
{
    double seconds = 0.0;
    const char *text = NULL;
    switch (parse_switch_number(argc, argv, "--soak", "a number of seconds",
                                &seconds, &text))
    {
    case SWITCH_NUMBER_ABSENT:
        return 0.0f;
    case SWITCH_NUMBER_BAD:
        return SOAK_MALFORMED;
    case SWITCH_NUMBER_READ:
        break;
    }
    if (seconds > 0.0)
        return (float)seconds;
    SDL_Log("--soak expects a positive number of seconds, not '%s'", text);
    return SOAK_MALFORMED;
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
 * page of `--screen settings`, and which of `--screen aftermath`'s poses.
 *
 * It named two counts here — "which half" of the options sheet and "four poses"
 * — and both had gone stale: the sheet is three pages since the records split and
 * the sweep walks five poses. Neither figure is held by anything, and neither
 * needs saying: `SETTINGS_PAGE_COUNT` is what `tools/soak.sh` already derives the
 * first from, and the second is a list in that script. A count in a comment is
 * prose, and prose about a number is the thing this tree keeps finding wrong.
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
 *
 * **Which is what it did**, and it is the same defect `--soak` had above, on the
 * switch whose whole job is reaching a drawing the default does not. The refusal
 * was printed and then nought was returned — the same answer as "no `--page`
 * given" — so `--screen manual --page abc` drew sheet one and `tools/soak.sh`
 * logged `manual sheet ok`. A sweep that says it walked ten sheets and walked the
 * first one ten times is this file's own recurring defect, *a check reporting
 * coverage it does not have*, and this is the switch that was written to end it.
 * `PAGE_MALFORMED` is the third answer, and `SDL_AppInit` refuses on it.
 */
#define PAGE_MALFORMED (-1)

static int parse_screen_page(int argc, char *argv[])
{
    double page = 0.0;
    const char *text = NULL;
    switch (parse_switch_number(argc, argv, "--page", "a sheet number", &page,
                                &text))
    {
    case SWITCH_NUMBER_ABSENT:
        return 0;
    case SWITCH_NUMBER_BAD:
        return PAGE_MALFORMED;
    case SWITCH_NUMBER_READ:
        break;
    }
    if (switch_number_is_index(page))
        return (int)page;
    /* `SDL_atoi` was what read this, so `--page 3s` opened sheet three and the
     * sweep logged `manual sheet ok` — which is the same "coverage it does not
     * have" this switch was written to end, reached by a typo instead of by a
     * missing switch. */
    SDL_Log("--page expects a whole sheet number of 1 or more, not '%s'", text);
    return PAGE_MALFORMED;
}

/*
 * `--shot PATH` writes the frame to a BMP file and closes, and the three
 * switches after it say which frame and how many: `--shot-at SECONDS`,
 * `--shot-frames N` and `--shot-fps F`.
 *
 * The one switch on this line that produces something rather than checking
 * something. All of the art is drawn at runtime, so there is no file anywhere in
 * this tree to put on a store page or in a bug report — the only place a picture
 * of this game exists is the back buffer. `tools/press_kit.sh` is the caller and
 * `ShotPlan` is where the design is written down.
 *
 * The three numeric ones share a reader rather than getting a loop each: four
 * parsers above are already four copies of the same walk over `argv`, and three
 * more of them to say "a number after a switch" would be the copy this file's own
 * comment on `SWITCHES[]` warns about.
 */
/*
 * Who is driving, answered before anything is opened or read.
 *
 * `game_init` applies the saved fullscreen and reads both of the player's
 * files, so this has to be known before it is called — which is why it asks
 * only whether the switch is *present* and leaves what its value means to the
 * parser that owns it further down. A malformed `--soak abc` is still a refusal
 * there; it is a scripted run either way, and a run that is about to be turned
 * down must not write anybody's settings on its way to the exit.
 *
 * Three switches, and `--screen` is the one worth arguing. It stages a *frame*
 * rather than starting a run: a cleared card on the last sector, a report with
 * numbers nobody scored, a floor a few seconds after it went wrong, a title
 * screen offering a resume nobody earned. `game_soak_screen`'s own comment
 * claims "nothing is banked and the record is not moved by a sweep" and that
 * was already only true of the sweep, which passes `--soak` beside it — a *hand*
 * running `--screen cleared` banks the sector its timer runs down into, which is
 * where this machine's `furthest_sector 2` and `best_score 3230` came from. And
 * `--screen resume` would have been worse in kind: it sets `furthest_sector`
 * itself, so on a player's run it writes a resume at the top of the tower to
 * their disk. A staged frame must not become somebody's save, so the switch that
 * stages one says so here instead of six places downstream trusting a comment.
 *
 * `--level` deliberately stays a player's run: it is the editor's playtest
 * button, and playtesting owes the author the game as they set it up. It is also
 * the one entry point here that *is* a run rather than a frame.
 *
 * Deliberately not a general "was this flag given" helper — every other switch
 * on the line is something a hand types at a game it means to play. See
 * `PlatformState.scripted`.
 */
static GameRunKind parse_run_kind(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        if (SDL_strcmp(argv[i], "--shot") == 0 ||
            SDL_strcmp(argv[i], "--soak") == 0 ||
            SDL_strcmp(argv[i], "--screen") == 0)
        {
            return GAME_RUN_SCRIPT;
        }
    }
    return GAME_RUN_PLAYER;
}

static const char *parse_shot_path(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        if (SDL_strcmp(argv[i], "--shot") != 0)
            continue;
        if (i + 1 >= argc)
        {
            SDL_Log("--shot needs a file to write after it");
            continue;
        }
        return argv[i + 1];
    }
    return NULL;
}

/* The number after `--shot-frames`, `--shot-fps` or `--shot-at`, or the default
 * that switch was left out. What each number has to be is `shot_plan_open`'s
 * business, so a refusal is stated once, beside the reason for it. */
/*
 * The third answer the three capture switches did not have.
 *
 * `SDL_atof` cannot fail: it answers nought to `abc` exactly as it answers
 * nought to `0`, and this function used to hand that straight back. Two of the
 * three switches got away with it by luck — `shot_plan_broke` refuses a frame
 * count under one and a rate under `MIN_FRAME_RATE`, so nought is out of range
 * and a typo was rejected for the wrong reason — and the third did not, because
 * **nought is a perfectly legal lead-in**. So `--shot-at abc` photographed frame
 * one, logged `Wrote 1 frame(s)`, exited nought, and put a file on disk.
 *
 * That is this file's own recurring defect on the one switch in the binary that
 * *produces* something rather than checking something. `--soak` and `--page`
 * were both given a `MALFORMED` answer for exactly this — see `SOAK_MALFORMED`
 * — and `--shot-at` arrived after that work and did not inherit it. It is worse
 * here than a missing check: `tools/soak.sh` counts the capture's files off the
 * disk rather than trusting its exit status, precisely so a capture that wrote
 * nowhere cannot pass, and a capture of the *wrong moment* writes a file and
 * sails through that too. What a mistyped lead-in produces is a press still or
 * a bug report of a frame nobody asked for, which nothing downstream can tell
 * from the frame that was asked for.
 *
 * A missing value is the same answer rather than a fallback, for the reason the
 * comment above it used to be wrong about: it `continue`d, so
 * `--shot --shot-at` quietly captured at the default and said only that it
 * "needs a number after it".
 *
 * **The answer comes back through a pointer rather than as a sentinel value**,
 * and the first draft of this got that wrong in a way worth keeping written
 * down: it returned `-1.0` for malformed, and `-1.0` is a number a command line
 * can perfectly well contain. So `--shot-at -1` — which
 * `shot_plan_broke` had always refused with "expects a number of seconds of
 * nought or more" — started being refused *silently* instead, the fix for one
 * bad message having eaten a good one. A sentinel inside the range of the thing
 * it stands for is not a sentinel; `SOAK_MALFORMED` gets away with it only
 * because a soak of minus one second is refused by the same branch and with the
 * same words either way.
 *
 * **And non-finite is refused here rather than downstream**, which closed a hole
 * that predates all of this. `nan` and `inf` are things `SDL_strtod` reads
 * happily and consumes whole, and every guard in `shot_plan_broke` is a `<`
 * comparison — all of which are false against a NaN. So `--shot-at nan` set a
 * lead-in that counts down forever and the process **never closed**: no capture,
 * no error, no exit. That is the failure every workflow under `.github` spends
 * its `timeout-minutes` on, produced by a switch whose only callers are scripts,
 * which is the same sentence this file already carries about `--soak`.
 *
 * **And that last clause was a description of a live bug rather than a
 * comparison**, which is why all of the above now lives in
 * `parse_switch_number` and this function is four lines and a range comment.
 * The paragraph was written here, about these three switches, and `--soak` went
 * on reading `SDL_atof` eighty lines up the file — so `--soak inf` did the
 * thing this paragraph describes, in the process, for as long as the paragraph
 * sat here explaining it. A rule stated in a comment beside one of its three
 * call sites is a rule holding one of them.
 */
static bool parse_shot_number(int argc, char *argv[], const char *name,
                              double fallback, double *out)
{
    *out = fallback;
    double value = 0.0;
    switch (parse_switch_number(argc, argv, name, "a number", &value, NULL))
    {
    case SWITCH_NUMBER_ABSENT:
        return true;
    case SWITCH_NUMBER_BAD:
        return false;
    case SWITCH_NUMBER_READ:
        break;
    }
    /* No range check here on purpose: what each of the three numbers has to be
     * is `shot_plan_open`'s business and is refused there, beside the reason.
     * This is only "was a number given at all", which is now the same question
     * every other switch on this line asks. */
    *out = value;
    return true;
}

/*
 * `--seed N` pins the night, so a capture can be taken twice.
 *
 * Two things in this process are seeded off the wall clock: the simulation's own
 * stream (`game_init` hands `time(NULL)` to `game_init_seeded`) and SDL's, which
 * the camera shake, the particles and the title screen's starfield draw from. So
 * the same command on the same commit produced a different picture every time —
 * a different live card, guards in different places, different decoration
 * variants — and `tools/press_kit.sh` says in its own header that the pictures
 * can be *rebuilt* after a change, which is a claim about repeatability that
 * nothing supported.
 *
 * It matters for the same reason `--shot-fps` does: a capture is a measurement.
 * The clock half of that was fixed by giving the presentation its own clock (see
 * `PresentationState.render_clock`) so a burst's animation no longer depends on
 * how fast the capturing machine drew it; this is the other half. With both, two
 * runs of the same capture are byte-identical, which is what lets a press still
 * be regenerated and diffed rather than re-cropped by eye.
 *
 * Strict, and here the strictness is not a nicety: **nought is a perfectly good
 * seed.** Every other numeric switch in this file gets away with a loose parse
 * because a typo decays to a value its own range check refuses — that is the luck
 * `--shot-frames` and `--shot-fps` were found relying on — and there is no range
 * to fall out of here, so `--seed abc` would silently pin seed nought and report
 * nothing. `SDL_strtoull` with the end pointer checked is what refuses it, and
 * `3s` with it.
 *
 * `SEED_NONE` is "not asked", which is the wall clock and the ordinary game.
 */
#define SEED_NONE 0
#define SEED_GIVEN 1
#define SEED_MALFORMED (-1)

static int parse_seed(int argc, char *argv[], Uint64 *out)
{
    for (int i = 1; i < argc; ++i)
    {
        if (SDL_strcmp(argv[i], "--seed") != 0)
            continue;
        if (i + 1 >= argc)
        {
            SDL_Log("--seed needs a number after it");
            return SEED_MALFORMED;
        }
        char *end = NULL;
        unsigned long long value = SDL_strtoull(argv[i + 1], &end, 10);
        if (end == argv[i + 1] || *end != '\0')
        {
            SDL_Log("--seed expects a whole number, not '%s'", argv[i + 1]);
            return SEED_MALFORMED;
        }
        *out = (Uint64)value;
        return SEED_GIVEN;
    }
    return SEED_NONE;
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
    static const char *const SWITCHES[] = {
        "--level",       "--soak",     "--screen",   "--page",
        "--shot",        "--shot-frames", "--shot-fps", "--shot-at",
        "--seed"};
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
        /* Spelled out of the same table the loop above reads, because the
         * sentence that used to be written here was a second copy of it and had
         * already gone stale: it named three switches while the table held four,
         * so a typo next to `--page` was answered by a message that did not know
         * the flag existed. */
        char list[192];
        list[0] = '\0';
        for (int s = 0; s < switch_count; ++s)
        {
            if (s > 0)
                SDL_strlcat(list, " ", sizeof(list));
            SDL_strlcat(list, SWITCHES[s], sizeof(list));
        }
        SDL_Log("Ignoring unknown argument '%s'; this build knows %s", argv[i],
                list);
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

    /* Read before the game is built, because the seed is the first thing
     * `game_init_seeded` spends and there is nothing to re-seed afterwards: the
     * live card and the decoration variants are drawn out of the stream while the
     * first level loads. */
    Uint64 seed = 0;
    int seeded = parse_seed(argc, argv, &seed);
    if (seeded == SEED_MALFORMED)
    {
        SDL_free(game);
        return SDL_APP_FAILURE;
    }

    /* Read beside the seed and for the same reason: both are spent inside
     * `game_init`, and there is nothing to put right afterwards. */
    GameRunKind run = parse_run_kind(argc, argv);

    if (!(seeded == SEED_GIVEN ? game_init_seeded(game, seed, run)
                               : game_init(game, run)))
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
    /* Read before the screen is staged rather than inside the call, because a
     * malformed sheet number is a refusal and not a default: see
     * `PAGE_MALFORMED`. */
    int page = parse_screen_page(argc, argv);
    if (page == PAGE_MALFORMED)
    {
        return SDL_APP_FAILURE;
    }
    if (screen != NULL)
    {
        GameScreenResult staged =
            game_soak_screen(game, screen, page, start_level);
        /*
         * The list goes with the one fault it describes, and it used to go with
         * all nine.
         *
         * `game_soak_screen` turns a request down for nine different reasons —
         * a sheet past the end of the manual, a page past the end of the options
         * sheet, a sector that cannot stage an aftermath, a restroom that will
         * not open — and each of those says so, in terms a caller can act on.
         * Then this printed the names anyway, so `--screen manual --page 99`
         * answered "Sheet 99 is outside the manual's 10" and immediately told
         * the caller their screen name was wrong, with `manual` sitting in the
         * list it had just printed. One `bool` fed one message: exactly the
         * SPAWNS parser's defect, on the switch whose whole job is telling a
         * script which names exist.
         *
         * The third place this list is written down, after `game_soak_screen`
         * and `tools/soak.sh` — and the one nothing held, which is how it came
         * to be printed here a release stale: `reveal` was accepted by the game
         * and named as not-a-screen by the refusal.
         * `tools/check_lists.py` holds all three now.
         */
        if (staged == GAME_SCREEN_UNKNOWN)
        {
            SDL_Log("--screen expects one of: abduction, chase, opening, "
                    "manual, settings, pause, report, cleared, reveal, "
                    "continue, gameover, outro, credits, resume, restroom, "
                    "aftermath, cover");
            return SDL_APP_FAILURE;
        }
        if (staged != GAME_SCREEN_STAGED)
            return SDL_APP_FAILURE;
    }

    /* After `--screen`, because the frame this captures is whatever that left on
     * the screen, and a capture opened first would still be pointed at the title
     * screen the staging replaced. */
    const char *shot = parse_shot_path(argc, argv);

    /*
     * Read into locals first, so a malformed one is refused on its own account
     * rather than on whether the value it decayed to happens to fall outside
     * `shot_plan_open`'s ranges. For the lead-in it does not — nought is a legal
     * lead-in — which is the whole of the bug this shape exists for. The messages
     * are logged as they are found, so the short circuit only decides which of
     * two typos gets named first.
     *
     * **Outside the `--shot` test rather than inside it**, which is where it was.
     * A typo in a value is a typo whether or not the switch it belongs to was
     * also given: `--shot-at abc` on its own printed nothing and exited nought,
     * while `--page abc` without a `--screen` has always refused. Same class,
     * two answers — and the caller here is a script, which is exactly the
     * argument `parse_soak_seconds` is written under: a switch a script drives
     * owes the script an exit code. Reading them unconditionally costs one pass
     * over `argv` on a line that does not mention them.
     */
    double frames = 0.0;
    double fps = 0.0;
    double lead = 0.0;
    if (!parse_shot_number(argc, argv, "--shot-frames", SHOT_FRAMES_DEFAULT,
                           &frames) ||
        !parse_shot_number(argc, argv, "--shot-fps", SHOT_RATE_DEFAULT, &fps) ||
        !parse_shot_number(argc, argv, "--shot-at", SHOT_LEAD_IN_DEFAULT,
                           &lead))
    {
        return SDL_APP_FAILURE;
    }
    if (shot != NULL)
    {
        if (!shot_plan_open(&game->platform.shot, shot, (int)frames,
                            (float)fps, (float)lead))
        {
            return SDL_APP_FAILURE;
        }
    }

    float soak = parse_soak_seconds(argc, argv);
    /* Before the budget is spent rather than after, and a refusal rather than a
     * fallback: see `SOAK_MALFORMED`. The message is already logged, so this
     * only has to carry the status out. */
    if (soak == SOAK_MALFORMED)
    {
        return SDL_APP_FAILURE;
    }
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
    /*
     * The one window event this game listens to, and the reason it now listens
     * to any: the simulation is advanced by `SDL_AppIterate` and not by input,
     * so a sector went on being played while its window was behind something
     * else. `SDL_EVENT_WINDOW_MINIMIZED` needs no line of its own — a minimised
     * window is not the focused one — and focus coming back deliberately does
     * not resume. See `game_pause_on_focus_lost`.
     */
    if (event->type == SDL_EVENT_WINDOW_FOCUS_LOST)
    {
        game_pause_on_focus_lost(game);
        return SDL_APP_CONTINUE;
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

    /* A capture runs the world on its own clock: one `1 / --shot-fps` step per
     * drawn frame, whatever this machine managed, so a burst is evenly spaced and
     * plays back at the rate it was asked for. Below the clamp because
     * `shot_plan_open` has already refused any rate the clamp would shorten. */
    float shot_dt = shot_plan_step_dt(&game->platform.shot);
    if (shot_dt > 0.0f)
    {
        elapsed = shot_dt;
    }
    shot_plan_advance(&game->platform.shot, elapsed);

    /* The presentation animates on this rather than on the wall clock, so a
     * capture's synthetic rate reaches the backdrop, the world and the strip as
     * well as the simulation. Banked here because this is where `elapsed` stops
     * changing. See `PresentationState.render_clock`. */
    game_advance_render_clock(game, elapsed);

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

    /* `game_render` is what writes a captured frame, so the plan can only be
     * spent once it has returned. A capture that could not be written closes as
     * a failure: a press run whose pictures are missing must not look like a
     * press run that worked. */
    if (shot_plan_complete(&game->platform.shot))
    {
        if (shot_plan_broke(&game->platform.shot))
        {
            return SDL_APP_FAILURE;
        }
        SDL_Log("Wrote %d frame(s) to %s; closing",
                game->platform.shot.frames, game->platform.shot.path);
        return SDL_APP_SUCCESS;
    }

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
