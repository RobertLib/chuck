#ifndef CHUCK_GAME_H
#define CHUCK_GAME_H

#include "audio.h"
#include "chase.h"
#include "common.h"
#include "credits.h"
#include "cutscene.h"
#include "gameplay_state.h"
#include "intro.h"
#include "manual.h"
#include "pad_hint.h"
#include "pause_sheet.h"
#include "particle.h"
#include "progress.h"
#include "screenshot.h"
#include "sector_tally.h"
#include "settings.h"

/*
 * How many poses `--screen aftermath --page N` can stage.
 *
 * Written here once because it was written down twice: the `switch (page)` in
 * `soak_stage_aftermath` and the pose list in [../tools/soak.sh](../tools/soak.sh),
 * which agreed on five while a comment above the list said four. The sweep
 * derives the figure off this line the way it already derives the options
 * sheet's page count off `SETTINGS_PAGE_COUNT`, so a pose is walked by having
 * been added rather than by somebody remembering to extend a bash array.
 *
 * A pose earns a number when it is the only route to a drawing — see the switch
 * for which renderer each one reaches.
 */
#define AFTERMATH_POSE_COUNT 7

typedef enum
{
    STATE_ABDUCTION,
    STATE_CHASE,
    STATE_OPENING_CUTSCENE,
    STATE_INTRO,
    STATE_MANUAL,
    STATE_SETTINGS,
    STATE_LEVEL_START,
    STATE_SHOW_KEYCARD,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_LEVEL_TRANSITION,
    STATE_LEVEL_CLEARED,
    STATE_OUTRO,
    STATE_CREDITS,
    STATE_CONTINUE,
    STATE_GAME_OVER
} GameState;

/* `PauseItem` and the rows it indexes are in [pause_sheet.h](pause_sheet.h),
 * which this header includes: the enum, the words and the count are generated
 * from one list so they cannot come to disagree, and the suite links no SDL and
 * so cannot reach this file to measure them. */

typedef struct
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Gamepad *gamepad;
    SDL_JoystickID gamepad_id;
    /* What the open pad prints on its face buttons, and where. Read once when
     * it is plugged in, because every prompt on every screen is spelled from
     * it and none of them should be asking a driver mid-frame. */
    PadHints pad;
    /* Which way the left stick was last pushed, as the d-pad button that means
     * the same thing, or `SDL_GAMEPAD_BUTTON_INVALID` for centred. A stick is
     * an axis and a menu wants presses, so the edge has to be remembered
     * somewhere; see `menu_stick_step` in
     * [game_input.c](game_input.c). */
    SDL_GamepadButton pad_menu_direction;
    AudioSystem audio;
    bool fullscreen;
    bool gamepad_active;
    Uint64 last_tick;

    /* Real time taken off the clock and not yet spent on a simulation step.
     * The step is fixed (`SIM_STEP_DT`) so that what the physics produces is a
     * property of the game rather than of the display it is drawn on; this is
     * where the remainder between two frames waits. */
    float sim_accumulator;

    /*
     * Whether the renderer actually took the vsync it was asked for, and what
     * to do about it if it did not.
     *
     * `SDL_SetRenderVSync` can refuse — a software renderer, a compositor that
     * ignores it, a VM — and the return value used to be dropped on the floor.
     * With nothing else limiting it `SDL_AppIterate` then runs as fast as the
     * machine can draw: a pinned core and a spun-up fan for a game that needs
     * neither. `frame_min_ns` is the floor the loop sleeps to when the swap is
     * not doing the waiting, and it is nought whenever vsync was granted.
     */
    bool vsync;
    Uint64 frame_min_ns;

    /*
     * How much wall-clock time this process has left before it closes itself,
     * and whether it was ever asked to.
     *
     * `--soak N` exists for one caller: `tools/soak.sh`, which runs the
     * **sanitized** build across every sector so that ASan and UBSan actually
     * execute the SDL half of the tree. `make sanitize` built
     * `build/chuck-sanitize` and then ran only `core_tests`, and CI could not
     * have run it either — that job builds SDL with no video backend at all —
     * so `game_render.c`, `level_art.c`, `cutscene.c`, `render_figures.c`,
     * `audio.c`, `intro.c`, `manual.c` and `chase_render.c` were
     * sanitizer-*compiled* and never sanitizer-*executed*. More than half the
     * tree, held by a job whose name says sanitizers.
     *
     * A budget rather than a signal from outside, because a soak that is killed
     * never reaches `SDL_AppQuit`: the teardown is the half of the lifecycle a
     * sanitizer is most likely to have something to say about, so the process
     * has to be able to end on its own.
     *
     * The countdown is spent in **raw** elapsed time, not the `MAX_FRAME_DT`
     * clamp below. Under a sanitizer a frame can take longer than the clamp,
     * and a soak that paid the clamped figure would ask for two seconds and sit
     * there for two minutes.
     */
    bool soaking;
    float soak_seconds_left;

    /*
     * Whether this run belongs to a script rather than to a person, which is
     * the whole of what decides that it touches nobody's disk.
     *
     * Set when `--shot` or `--soak` is on the line, because those two switches
     * have no other caller: one produces an artifact and one exits by itself,
     * and both were reading and writing the settings and progress of whoever
     * ran them. That is not a small leak. `game_init` applies the saved
     * `fullscreen`, `screenshot_write` reads back the *window* rather than the
     * logical frame, and the two together meant a capture came out at the
     * runner's display size: measured, 800x552 with the flag off and 1024x706
     * with it on, the second pre-scaled by a non-integer 1.28 — which is
     * exactly the pixel-art damage `tools/press_kit.sh` goes to trouble to
     * avoid, applied before its own `-resize 200%` ever runs. The MANIFEST it
     * writes into every press kit said the captures were taken "at the window
     * the game opens (800x552) and the settings it ships with", while the same
     * script's header said it "reads and writes the settings and progress of
     * whoever runs it". Both sentences in one file, seventy lines apart, and
     * only one of them could be true.
     *
     * This is the argument `--seed` is already written under, finished: a
     * capture is a measurement and a measurement has to be repeatable, and a
     * seed only pins the half of the frame that comes out of the RNG. The other
     * half is the runner's own CRT filter, reduced motion, assists and records.
     *
     * The other direction matters just as much and is what the soak sweep was
     * doing: `make soak` and `make sanitize` banked their own numbers into the
     * developer's `progress.cfg` — `--screen cleared` finishes a sector — so a
     * test gate quietly rewrote the player's records. A gate must not.
     *
     * A run driven by a hand is left exactly as it was: `--level` is the
     * editor's playtest button and the title screen is somebody's evening, and
     * both of those are the player's display and the player's save.
     */
    bool scripted;

    /* The capture `--shot` asked for, if any. Beside the soak budget because it
     * is the same kind of request; they spend different clocks on purpose, and
     * `ShotPlan` says why. */
    ShotPlan shot;

    /*
     * Whether the frame on screen was staged by `--screen` rather than played
     * into.
     *
     * `soak_freeze_staged_frame` holds a staged world still by entering
     * `STATE_PAUSED` — the one state whose own comment is "time stands still" —
     * which also draws the pause sheet over it. Right state, wrong picture: the
     * aftermath exists to *show* the bodies, the alarm light and the crawl, and
     * all of them were behind a menu. Nothing is lost by skipping the sheet here,
     * because `--screen pause` is the name that draws it.
     */
    bool staged_frame;
} PlatformState;

typedef struct
{
    float message_timer;

    /* Particle system for effects */
    ParticleSystem particles;

    float footstep_audio_timer;
    float ladder_audio_timer;
    bool footstep_alternate;
    bool fall_platform_sounded[MAX_FALL_PLATFORMS];

    /* How much of the landing squash the player figure has left to play, 1 down
     * to 0. It is a weight cue and nothing else — the shell derives it from the
     * fall speed the physics step already reports, so no gameplay module has to
     * know the figure compresses when it lands. */
    float player_land_squash;

    /* The three beats of the prologue, in the order they play: the kerb where
     * she is taken, the drive across town (in `Chase`), and the pavement
     * outside the tower she is walked into. */
    AbductionCutscene abduction_cutscene;
    OpeningCutscene opening_cutscene;

    /* Results report and the captors' next flight of stairs, between levels. */
    LevelTransition level_transition;

    /* Final rooftop rescue and happy ending after the last level, and the roll
     * of names it hands to on its way back to the title screen. */
    OutroCutscene outro_cutscene;
    CreditsRoll credits;

    /* Title-screen state (field-operations briefing shown before STATE_LEVEL_START) */
    Intro intro;

    /* The field manual, opened from the title screen. */
    Manual manual;

    float cam_x; /* world x of left edge of the viewport */
    float cam_y; /* world y above the visible gameplay area */
    /* Short, decaying render offset used by gameplay explosions. The HUD is
     * rendered separately and therefore stays readable while the world shakes. */
    float camera_shake_timer;
    float camera_shake_duration;
    float camera_shake_strength;
    float camera_shake_x;
    float camera_shake_y;
    /* Key-card intro animation state (used between reveal and playing) */
    int card_anim_current;     /* current highlighted position */
    int card_anim_step;        /* steps advanced in animation */
    int card_anim_total_steps; /* total steps to run before stopping */
    int card_anim_count;       /* number of card positions */
    float card_anim_interval;  /* seconds between highlight steps */
    float card_anim_timer;     /* accumulator for highlight timing */
    float exit_unlocked_timer; /* seconds to show "EXIT UNLOCKED" overlay */
    float extra_life_timer;    /* seconds to flash the score-earned 1UP */

    /* The last thing anybody on the crew said within earshot, and how long is
     * left to read it. Only the shell holds it: the simulation reported that
     * somebody spoke and drew a number, and the words are looked up out of
     * [crew.c](crew.c) at draw time. A second line inside the window replaces
     * the first outright rather than queueing — the plate is one line high,
     * and a backlog of overheard traffic would still be printing the lobby's
     * jokes two rooms later. */
    ChatterKind chatter_kind;
    int chatter_speaker;
    int chatter_roll;
    float chatter_timer;

    /* What the sector just cleared paid, when it cleared by a route that shows
     * no report — a window, or the last sector of the campaign. See
     * [sector_tally.h](sector_tally.h): the bonus and the record were being
     * handed out on eleven of the seventeen clears with nothing on screen to
     * connect either to. */
    SectorTally sector_tally;

    /*
     * The clock the presentation animates on.
     *
     * Banked once per drawn frame from the same `elapsed` the simulation is fed,
     * which is the whole point of it: five things in
     * [game_render.c](game_render.c) — the backdrop, the interior world, the
     * facade world, the ACCESS lamp and the TRAIL meter — read
     * `SDL_GetTicksNS()` directly, and a capture replaces `elapsed` with a
     * synthetic `1 / --shot-fps` step so that a burst plays back at the rate it
     * was asked for. Those five did not get the message. Measured over a
     * five-frame burst, the world moved by 40, 17 and 6 pixels of difference at
     * 20, 60 and 200 fps — scaling with the rate as designed — while the TRAIL
     * meter moved 12, 13 and 17, which is to say by however long the machine
     * happened to take. So `make press` produced GIFs whose HUD and backdrop
     * animate at the capture host's speed rather than the GIF's, and rebuilding
     * the press kit on another machine produced different pictures from the same
     * commit.
     *
     * It is a wall clock rather than a simulation clock on purpose: it is
     * advanced whatever state the game is in, so the backdrop and the strip keep
     * breathing behind a pause sheet the way they always have. What changes is
     * only that a capture's frames are now spaced by the capture's own rate.
     */
    float render_clock;
} PresentationState;

typedef struct
{
    PlatformState platform;
    CampaignState campaign;
    GameplayState gameplay;
    /* The prologue pursuit. It runs once, between the title screen and the
     * opening cutscene, and is a self-contained simulation of its own rather
     * than a variation on the platformer. */
    Chase chase;
    /* The inactive half of a sublevel visit. It holds the paused main level
     * while Chuck is inside, and the persistent restroom state while outside. */
    GameplayState inactive_gameplay;
    bool sublevel_initialized;
    bool in_sublevel;
    float main_level_cam_x;
    float main_level_cam_y;
    /* The parked half of PresentationState.fall_platform_sounded. It is keyed
     * by a panel of one particular level, so it changes hands with that level
     * rather than staying behind with the shell. */
    bool inactive_fall_platform_sounded[MAX_FALL_PLATFORMS];
    PresentationState presentation;
    Input input;
    GameState state;
    /* Where the pause and the assist sheet return to. Pausing must resume the
     * exact state it interrupted (re-entering STATE_LEVEL_START would replay
     * the reveal), so the return path is a stored state, not a transition. */
    GameState pause_return_state;
    GameState settings_return_state;
    /* Everything the player has decided, and where each sheet's cursor is
     * standing. The settings survive a campaign reset and are written back to
     * disk when the sheet is closed, so a run is never what a preference
     * belongs to. */
    Settings settings;
    /* Which of the sheet's three pages is open, where its cursor is standing,
     * and — on the controls page — which of the row's two key slots the caret is
     * on and whether the next key pressed is being taken rather than obeyed. */
    SettingsPage settings_page;
    int settings_cursor;
    int settings_bind_slot;
    bool settings_capturing;
    /*
     * Which action row has been pressed once and is waiting to be pressed again,
     * or `SETTING_NONE`.
     *
     * A row whose action cannot be undone is armed rather than taken — the same
     * reasoning that keeps the pause cursor off ABANDON RUN. Anything other than
     * a second press on the same row disarms it, which is what the armed detail
     * line promises; `settings_disarm_action_row` is the one place that clears it
     * so no input path can leave the sheet armed behind the player's back.
     *
     * **It was a `bool` naming the records row, and that is why the second such
     * row shipped without an arm at all.** `RESET CONTROLS` destroys as much and
     * asked nothing; see `SETTINGS_RECORDS_ARMED_DETAIL` in
     * [settings.h](settings.h). An id rather than a flag per row means the sheet
     * has one thing to remember however many rows arm, and which rows those are
     * is `settings_row_armed_detail`'s answer rather than a list kept here.
     */
    SettingId settings_armed_row;
    int pause_cursor;
    /*
     * The pause sheet's ABANDON row, armed rather than taken — the same shape as
     * `settings_armed_row` above and for a stronger reason: that row throws
     * away records the player can rebuild, this one throws away the run they are
     * standing in. See `PAUSE_ABANDON_ARMED` in [pause_sheet.h](pause_sheet.h)
     * for the two shortcuts that used to reach it on a single press, one of them
     * the default weapon-cycle key.
     *
     * Cleared by opening the sheet, by moving the cursor and by leaving; the
     * cursor moving off the row is what "changed my mind" looks like, so nothing
     * has to be pressed to disarm it.
     */
    bool pause_abandon_armed;
    /*
     * Where the manual goes back to: `STATE_INTRO` or `STATE_PAUSED`.
     *
     * The twin of `settings_return_state`, and it exists for the same reason the
     * manual can now be opened from a paused run at all — see
     * `game_open_manual`. Without it every "done" key would hand a mid-sector
     * reader to `game_return_to_intro`, which banks the score and ends the run.
     */
    GameState manual_return_state;
    /* What outlives the process: the best score any run has finished on, and
     * the furthest sector one has reached. Written on the frames that move a
     * number and read once at startup, so a campaign that takes more than one
     * sitting can be picked up from the title screen. */
    Progress progress;
    /* Set by the title screen's quit chip and read once a frame by
     * SDL_AppIterate. The chip is reached by holding the pad's B, by clicking
     * it, or by ESC, and none of those three sit where they could return an
     * SDL_AppResult of their own — so the answer is parked here rather than
     * threaded back out through three different call paths. */
    bool quit_requested;
#ifdef CHUCK_DEBUG
    int debug_selected_level;
#endif
} Game;

/*
 * Who is driving, which the two `game_init` entry points below need before they
 * open a window or read a file. See `PlatformState.scripted` for why it is a
 * question about the run rather than a question about the switch.
 *
 * Spelled as an enum rather than passed as a `bool`, so that `game_init(game,
 * GAME_RUN_SCRIPT)` says at the call site which of the two a reader is looking
 * at. There is one call site and it is in [main.c](main.c).
 */
typedef enum
{
    GAME_RUN_PLAYER = 0,
    GAME_RUN_SCRIPT
} GameRunKind;

bool game_init(Game *game, GameRunKind run);
bool game_init_seeded(Game *game, uint64_t seed, GameRunKind run);
void game_handle_event(Game *game, const SDL_Event *event);
void game_update(Game *game, float dt);
void game_render(Game *game);
void game_shutdown(Game *game);

/* Abort the current game (if any) and go back to the title screen. */
void game_return_to_intro(Game *game);

/* Open the field manual. It is read from the title screen, and closed with the
 * same route back as anything else: game_return_to_intro. */
void game_open_manual(Game *game);
/* Put the manual back over whatever it was opened from — the title screen, or
 * the pause sheet of a run still standing. Not `game_return_to_intro`, which
 * banks the score and ends the run. */
void game_close_manual(Game *game);

/* The pause menu: three items, walked with the cursor and answered with
 * confirm. Pausing resumes the exact state it interrupted. */
void game_toggle_pause(Game *game);
void game_pause_move_cursor(Game *game, int delta);
/* Put the cursor on ABANDON RUN and arm it rather than taking it; a second call
 * with it already armed abandons. `Q` and the pad's `SELECT`, which both used to
 * drop a run on one press. See `PAUSE_ABANDON_ARMED` in pause_sheet.h. */
void game_pause_arm_abandon(Game *game);
void game_pause_activate(Game *game);
/* Pause because the window stopped being the one in front. See the note in
 * [game.c](game.c): the world runs off `SDL_AppIterate` and not off input, so
 * this is the difference between alt-tabbing and losing a life. */
void game_pause_on_focus_lost(Game *game);

/* The options sheet. It opens from the title screen or from pause and returns
 * to whichever opened it, and every change it makes is felt at once: a volume
 * reaches the mixer, fullscreen reaches the window, an assist switch reaches
 * whatever is running. Closing it writes the file. */
void game_open_settings(Game *game);
void game_close_settings(Game *game);
void game_settings_move_cursor(Game *game, int delta);
void game_settings_adjust(Game *game, int delta);
/*
 * ENTER on the sheet. It opens the controls page, resets the bindings, or arms
 * the capture, depending on the row — and on a slider or a toggle it is a
 * change input like any other, which is why it is one call rather than a test
 * at every key that means "yes".
 */
void game_settings_confirm(Game *game);
/*
 * A key pressed while the capture is armed. Returns true when the press was
 * swallowed by the sheet, which is every press once it is armed: the whole
 * point is that the next key means itself rather than what it is bound to.
 * `scancode` of an unbindable key cancels, which is what makes ESC the way
 * out of a capture as well as out of the sheet.
 */
bool game_settings_capture_key(Game *game, int scancode);
/*
 * And the pad's half of the same row. `button` is already the positional value
 * the file keeps — the caller resolves the physical press through the pad's
 * own letters first — so nothing about which controller is plugged in crosses
 * this line. An unbindable button cancels, which is how START and BACK come to
 * be the pad's way out of a capture.
 */
bool game_settings_capture_pad(Game *game, int button);
/* True when the caret is on one of the row's pad caps rather than its keys.
 * The two captures are separate tables and this is what decides which one a
 * press is offered to. */
bool game_settings_slot_is_pad(const Game *game);
/* Back out of the controls page to the main one; false when there is nothing
 * to back out of, so the caller can close the sheet instead. */
bool game_settings_leave_page(Game *game);

/* Fullscreen, from the sheet or from F. One function, because the window and
 * the saved setting must never disagree about which the player asked for. */
void game_set_fullscreen(Game *game, bool on);

/* Write the settings file. The sheet does it on the way out; so does shutdown,
 * which is what catches a fullscreen toggled with F mid-run. */
void game_save_settings(const Game *game);

/* Start a clean campaign directly in one embedded level, skipping the title
 * screen and the prologue. The debug level picker uses it, and so does the
 * `--level N` switch the editor's playtest button launches the game with. */
bool game_start_at_level(Game *game, int level_index);

/*
 * Put the game on one named screen and leave it there. `--screen NAME`, and
 * `tools/soak.sh` is its only caller.
 *
 * `--level N` reaches the sectors and the title screen reaches itself, and
 * between them that is what the soak used to cover — the script said it also
 * reached the prologue and it never did, because the title screen advances on
 * a key press and a headless run receives none. Everything drawn only after a
 * menu choice, a cleared sector or a finished campaign was therefore compiled
 * under the sanitizers and never executed by them.
 *
 * The names, and the states they land on:
 * `abduction`, `chase`, `opening` — the prologue's three beats;
 * `manual`, `settings`, `pause` — the three sheets a player opens;
 * `report`, `cleared`, `continue`, `gameover` — the four cards a sector or a
 * run ends on;
 * `outro`, `credits` — the ending and the roll after it;
 * `restroom` — the sublevel behind a `U`, which no `--level` run enters on its
 * own;
 * `aftermath` — a sector a few seconds after it went wrong, which is the one
 * thing no `--level` run reaches because a headless player never acts: the
 * bodies, the opened patch, the alarm lighting, the particles and the bazooka.
 * See `soak_stage_aftermath`.
 *
 * `GAME_SCREEN_UNKNOWN` for a name this build does not know, and
 * `GAME_SCREEN_REFUSED` — with a line already logged saying which — for a name it
 * knows and a request it cannot honour. Two answers rather than one, because one
 * `bool` fed one message: `game_soak_screen` turns a request down for nine
 * different reasons and only one of them is the name, so `--screen manual --page
 * 99` printed "Sheet 99 is outside the manual's 10" and then had its caller
 * append the list of screen names — telling the caller their screen name was
 * wrong while `manual` sat in the list it had just printed. That is the SPAWNS
 * parser's own defect on the one switch whose whole job is telling a script which
 * names exist. A malformed name and an impossible request are two faults and want
 * two sentences.
 */
typedef enum
{
    GAME_SCREEN_STAGED = 0,
    /* This build has no screen by that name; the caller owes the list. */
    GAME_SCREEN_UNKNOWN,
    /* The name is real and the request is not. Whatever refused it has already
     * said so, and in terms the caller can act on. */
    GAME_SCREEN_REFUSED
} GameScreenResult;

/*
 * `page` is the sheet `--page N` asked for, 1-based, or nought for "whatever the
 * screen opens on". Only the manual reads it — it is the one screen name that
 * stands for ten drawings — and a number past the end of the sheaf is refused
 * rather than clamped, for the reason a sector outside the campaign is.
 *
 * `level_index` is the sector `--level N` named, 0-based, or negative for "the
 * screen's own choice", which is sector 1. The screens that draw over a live
 * world load one, and which one it is used to be sector 1 always — **which made
 * `restroom` one name standing for four maps, exactly as `manual` is one name
 * standing for ten drawings.** Which room a `U` opens on is decided by the
 * sector's `THEME` (`level_theme_sublevel`), so a sweep pinned to sector 1 drew
 * `restroom_lobby` seventeen times over and left the plant's, the archive's and
 * the penthouse's rooms sanitizer-compiled and never sanitizer-executed. The
 * toilet prop `q` is in those three and in no other map in the game, so
 * `draw_restroom_toilet` was a drawing function nothing in the tree ever ran.
 * `--page` answered the sheaf and this answers the rooms; the sectors to walk
 * are the ones with a `U` in them, which the sweep greps out of the maps rather
 * than keeping a list of.
 */
GameScreenResult game_soak_screen(Game *game, const char *name, int page,
                                  int level_index);

/*
 * Bank one drawn frame's worth of time on the presentation clock.
 *
 * Called from `SDL_AppIterate` with the same `elapsed` the simulation is about
 * to be stepped by — after the frame-time clamp and after a capture has
 * substituted its own step, so what the animations see is what the world sees.
 * See `PresentationState.render_clock`.
 */
void game_advance_render_clock(Game *game, float elapsed);

/* The furthest sector any run has reached, 0-based, and 0 when nobody has got
 * past the lobby — which is also what the title screen reads to decide whether
 * it offers a resume at all. */
int game_resume_sector(const Game *game);
/* The most of the docket any run has ever come away with. Read by the one
 * screen that looks at a finished run. */
int game_best_evidence(const Game *game);
/*
 * The per-sector records, as an array `CAMPAIGN_SECTORS` long.
 *
 * `progress_sector_time` answers one sector at a time, which is right for the
 * report and wrong for the sheet that shows all of them: THE RECORD would have
 * to know how `Progress` stores them to walk it. This hands over the run of
 * floats it already keeps, so the manual reads a list and knows nothing else.
 */
const float *game_sector_records(const Game *game);

/* The best score any finished run has left behind. Drawn on the game-over
 * card, which is the one screen a score is being looked at on. */
int game_best_score(const Game *game);

/* Take the resume the title screen is offering: a clean run of the furthest
 * sector reached, prologue and all the floors below it skipped. False when
 * there is nothing to resume or this is not the title screen. */
bool game_resume_campaign(Game *game);

/* Helper: obtain current view size (logical or window). Exposed to render
 * module so rendering and camera code can share the same behavior. */
void game_get_view_size(Game *game, int *out_w, int *out_h);

/* Open/close the first available gamepad and track hot-plug events. */
void game_input_init(Game *game);
void game_input_shutdown(Game *game);

/* The pad every prompt is spelled for, or NULL when the keyboard is the thing
 * in the player's hands. One answer, so no two screens can disagree about
 * which set of hints the frame is wearing. */
const PadHints *game_pad_hints(const Game *game);

/* And the set to spell a stored binding with, which is never NULL: see the
 * note beside it in [game_input.c](game_input.c). */
const PadHints *game_pad_spelling(const Game *game);

/* Read current keyboard and gamepad state into `game->input`. */
void game_read_input(Game *game);

#endif /* CHUCK_GAME_H */
