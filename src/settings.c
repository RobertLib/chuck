#include "settings.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * The sheet. Three sections, in the order a player looks for them: what they
 * can hear, what they are looking at, and what the game will do for them if
 * they ask. Assist is last and keeps its own sentence, because the three
 * switches under it are the only ones that change what the game is rather than
 * how it is delivered.
 */
static const SettingRow ROWS[] = {
    {SETTING_ROW_HEADING, SETTING_NONE, "AUDIO", NULL},
    {SETTING_ROW_SLIDER, SETTING_MUSIC_VOLUME, "MUSIC",
     "ONE SCORE PER SECTOR"},
    {SETTING_ROW_SLIDER, SETTING_SFX_VOLUME, "SOUND EFFECTS",
     "SHOTS, STEPS, DOORS AND THE CREW'S NET"},

    {SETTING_ROW_HEADING, SETTING_NONE, "DISPLAY", NULL},
    {SETTING_ROW_TOGGLE, SETTING_FULLSCREEN, "FULLSCREEN",
     "ALSO ON F, ANYWHERE IN THE GAME"},
    {SETTING_ROW_TOGGLE, SETTING_CRT_FILTER, "CRT FILTER",
     "SCANLINES AND VIGNETTE OVER THE PICTURE"},
    {SETTING_ROW_TOGGLE, SETTING_REDUCED_MOTION, "REDUCED MOTION",
     "NO SCREEN SHAKE; WARNING LIGHTS GLOW INSTEAD OF STROBING"},
    /*
     * The three rows that open the other pages, under a heading of their own.
     *
     * `CONTROLS` used to hang off the end of DISPLAY and `RECORDS` off the end of
     * CHALLENGE, which put two "this opens another sheet" rows inside two
     * sections about something else — a grouping nobody chose, arrived at because
     * the page had no room for a fourth heading. It has room now.
     */
    {SETTING_ROW_HEADING, SETTING_NONE, "MORE", NULL},
    {SETTING_ROW_ACTION, SETTING_OPEN_CONTROLS, "CONTROLS",
     "WHICH KEY AND BUTTON DOES WHAT, ON ITS OWN SHEET"},
    {SETTING_ROW_ACTION, SETTING_OPEN_DIFFICULTY, "DIFFICULTY",
     "ASSIST AND VETERAN, ON THEIR OWN SHEET"},
    {SETTING_ROW_ACTION, SETTING_OPEN_RECORDS, "RECORDS",
     "WHAT THE GAME KEEPS BETWEEN RUNS, ON ITS OWN SHEET"},
};

/*
 * The second page: the nine sector controls and the row that puts them back.
 *
 * A binding row carries no detail line of its own, and that is the one place
 * this table departs from "every row explains itself". The label *is* the
 * explanation — MOVE LEFT says the whole of what the row does — and nine
 * sentences repeating it would push the page past the frame for no reader's
 * benefit. What the section needs saying is said once, on the heading.
 */
static const SettingRow CONTROL_ROWS[] = {
    {SETTING_ROW_HEADING, SETTING_NONE, "ON FOOT",
     "LEFT/RIGHT PICKS A CAP: TWO KEYS, THEN TWO PAD BUTTONS."},
#define CHUCK_BIND_SHEET_ROW(ident, label, key)                              \
    {SETTING_ROW_BINDING, (SettingId)(SETTING_BIND_FIRST + BIND_##ident),    \
     label, NULL},
    CHUCK_BIND_LIST(CHUCK_BIND_SHEET_ROW)
#undef CHUCK_BIND_SHEET_ROW
    {SETTING_ROW_ACTION, SETTING_BINDINGS_RESET, "RESET CONTROLS",
     "PUT THE KEYBOARD AND THE PAD BACK TO WHAT THEY WERE"},
};

/*
 * The third page: how hard the night is, in both directions.
 *
 * These were two sections at the bottom of the main page until that page ran out
 * of height — one value row in hand, none with the mute warning up. They come off
 * it together rather than one at a time because they are one question: ASSIST is
 * the game apologising for its tuning and CHALLENGE is the same lever the other
 * way, `gameplay_enemy_speed_scale` reads both and resolves them against each
 * other, and a sheet with one of the two on it would be a sheet that answers half
 * of "how hard do you want this".
 */
static const SettingRow DIFFICULTY_ROWS[] = {
    {SETTING_ROW_HEADING, SETTING_NONE, "ASSIST",
     "THE GAME IS TUNED WITHOUT THESE. TAKE WHAT HELPS."},
    {SETTING_ROW_TOGGLE, SETTING_MORE_HEARTS, "MORE HEARTS",
     "FIVE HEARTS PER LIFE INSTEAD OF THREE"},
    {SETTING_ROW_TOGGLE, SETTING_SLOWER_GUARDS, "SLOWER GUARDS",
     "GUARDS AND DOGS MOVE AT 80% SPEED"},
    {SETTING_ROW_TOGGLE, SETTING_INFINITE_LIVES, "INFINITE LIVES",
     "A DEATH NEVER COSTS A LIFE"},

    {SETTING_ROW_HEADING, SETTING_NONE, "CHALLENGE",
     "FOR THE CLIMB YOU ALREADY KNOW BY HEART."},
    /*
     * `THIS RUN TOO` rather than `NEXT RUN`, which is what it said and which was
     * not true of it.
     *
     * Two of the three numbers reach a run in progress. The pace does so on
     * purpose and always did — `state->veteran` is set by
     * `apply_assist_to_state` and read by `gameplay_enemy_speed_scale` on the
     * next simulation step. The lives do too, and that half was nobody's
     * intention: `campaign_accept_continue` is the *other* place lives are
     * handed out, it reads `campaign->veteran`, and that flag follows this
     * switch — so flipping VETERAN on mid-run cuts the next continue from
     * `PLAYER_LIVES` to `VETERAN_LIVES`. Only the continue *count* is genuinely
     * `campaign_reset`'s and so genuinely next-run.
     *
     * `docs/screens.md` and `test_the_veteran_run_is_three_numbers_and_no_more`
     * both describe and require the live behaviour, and the behaviour is right:
     * a veteran run whose continue handed back three lives is the mode expiring
     * on first contact, which is the bug that put the flag on `CampaignState` in
     * the first place. What was wrong was this line and the comment beside
     * `case SETTING_VETERAN:` in game.c — the two places a *player* and the next
     * reader look, telling them it was safe to flip. Same shape as the `$A` pad
     * cap: the sheet reporting something the simulation does not do.
     */
    {SETTING_ROW_TOGGLE, SETTING_VETERAN, "VETERAN",
     "FASTER CREW, ONE LIFE, NO CONTINUES. THIS RUN TOO."},
};

/*
 * The third page, and the section that exists because of what the assist
 * switches do to the two figures the game keeps.
 *
 * A run with any assist on banks no score, no sector time and no docket
 * (`campaign_records_count`), which is the only arrangement under which the
 * per-sector times mean anything at all — a par set with infinite lives is a par
 * nobody can beat honestly, and the player who set it was never told. The strap
 * says so, which is where a sentence about the whole page belongs; the row is how
 * somebody who found that out too late gets their sheet back.
 *
 * It was a fifth section on the main page, and that page has no room for one.
 */
static const SettingRow RECORD_ROWS[] = {
    {SETTING_ROW_HEADING, SETTING_NONE, "WHAT THE GAME KEEPS",
     "THE BEST SCORE, THE DOCKET, AND EVERY SECTOR'S BEST TIME."},
    /*
     * And it shows them, which for a release it did not.
     *
     * The strap above listed the three things the game keeps and the only other
     * row on the page offered to delete all of it, so the one screen in the game
     * whose subject is the records was the one screen that would not print one —
     * they were readable on the field manual's `THE RECORD` sheet and nowhere
     * else. A page that asks "are you sure" about numbers it will not show is
     * asking about nothing.
     *
     * The four figures are `RunTallyRecord`, formatted in
     * [run_tally.c](run_tally.c) beside the manual's own cell: the same file
     * answers what a record reads as wherever it is read. The per-sector times
     * stay on the manual sheet — seventeen of them are a grid, not a row — and
     * `SECTORS TIMED` is the line that tells a player there is a grid to go and
     * look at.
     */
    /*
     * **A readout's label is left NULL on purpose**, and resolved through
     * `settings_row_label` from [run_tally.c](run_tally.c).
     *
     * These four rows used to spell the labels out again — "BEST SCORE",
     * "DOCKET", "FURTHEST FLOOR", "SECTORS TIMED" — beside a `RECORD_LABELS`
     * table in run_tally.c that says the same four things, is guarded by a
     * `_Static_assert` and asserted on by the suite, and **had no caller in the
     * game at all**. So the file whose stated job is that "the same file answers
     * what a record reads as wherever it is read" held a table nothing drew, the
     * check sat on the copy nobody could see, and the copy the player actually
     * reads was held by nothing — which is how one of the four came to name a
     * unit its own value does not use.
     *
     * That is this repository's own recurring defect twice over in one place: a
     * list written down twice, and a check reporting coverage it does not have.
     * The words have one home now and the label reaches the renderer from it, so
     * a fifth figure cannot arrive with a name in one file and a different name
     * in the other.
     */
    {SETTING_ROW_READOUT, SETTING_READOUT_FIRST + RUN_TALLY_RECORD_SCORE,
     NULL, "THE MOST ANY FINISHED RUN HAS COME AWAY WITH"},
    {SETTING_ROW_READOUT, SETTING_READOUT_FIRST + RUN_TALLY_RECORD_DOCKET,
     NULL, "SHEETS THE BEST SINGLE NIGHT CARRIED OUT"},
    {SETTING_ROW_READOUT, SETTING_READOUT_FIRST + RUN_TALLY_RECORD_FURTHEST,
     NULL, "THE HIGHEST SECTOR ANY RUN HAS REACHED"},
    {SETTING_ROW_READOUT,
     SETTING_READOUT_FIRST + RUN_TALLY_RECORD_SECTORS_TIMED, NULL,
     "EVERY ONE OF THEM IS ON THE RECORD SHEET IN THE MANUAL"},
    /*
     * The one destructive row in this table that the caret cannot be kept off,
     * and it is worth saying why rather than leaving it to look like an
     * oversight.
     *
     * `game_toggle_pause` opens the pause cursor on RESUME on the argument that
     * "the one item on this list that cannot be undone must never be the one
     * sitting under the thumb". This page cannot honour that: the four rows above
     * are readouts, `settings_row_is_reachable` steps the caret over them, so
     * `settings_first_row` has exactly one row to land on and this is it. Adding
     * a row for the caret to rest on instead would be a row that does nothing,
     * on the sheet whose own rule is that every reachable row says what it is
     * for.
     *
     * So the arm is load-bearing here in a way it is not anywhere else — it is
     * the whole of the guard rather than the second half of one — which is the
     * reason `settings_row_armed_detail` is a question about the table rather
     * than a flag somebody sets per row.
     */
    {SETTING_ROW_ACTION, SETTING_RECORDS_RESET, "RESET RECORDS",
     "CLEARS THE BEST SCORE, EVERY SECTOR TIME AND THE DOCKET"},
};

#define ROW_COUNT ((int)(sizeof(ROWS) / sizeof(ROWS[0])))
#define CONTROL_ROW_COUNT                                                     \
    ((int)(sizeof(CONTROL_ROWS) / sizeof(CONTROL_ROWS[0])))
#define RECORD_ROW_COUNT                                                      \
    ((int)(sizeof(RECORD_ROWS) / sizeof(RECORD_ROWS[0])))
#define DIFFICULTY_ROW_COUNT                                                  \
    ((int)(sizeof(DIFFICULTY_ROWS) / sizeof(DIFFICULTY_ROWS[0])))

const SettingRow *settings_rows(SettingsPage page, int *out_count)
{
    if (page == SETTINGS_PAGE_CONTROLS)
    {
        if (out_count != NULL)
            *out_count = CONTROL_ROW_COUNT;
        return CONTROL_ROWS;
    }
    if (page == SETTINGS_PAGE_DIFFICULTY)
    {
        if (out_count != NULL)
            *out_count = DIFFICULTY_ROW_COUNT;
        return DIFFICULTY_ROWS;
    }
    if (page == SETTINGS_PAGE_RECORDS)
    {
        if (out_count != NULL)
            *out_count = RECORD_ROW_COUNT;
        return RECORD_ROWS;
    }
    if (out_count != NULL)
        *out_count = ROW_COUNT;
    return ROWS;
}

const char *settings_page_title(SettingsPage page)
{
    switch (page)
    {
    case SETTINGS_PAGE_CONTROLS:
        return "CONTROLS";
    case SETTINGS_PAGE_DIFFICULTY:
        return "DIFFICULTY";
    case SETTINGS_PAGE_RECORDS:
        return "RECORDS";
    case SETTINGS_PAGE_MAIN:
    case SETTINGS_PAGE_COUNT:
        break;
    }
    return "OPTIONS";
}

const char *settings_page_strap(SettingsPage page)
{
    switch (page)
    {
    case SETTINGS_PAGE_CONTROLS:
        return "ENTER, THEN THE NEW KEY OR BUTTON. ESC AND START CANCEL.";
    case SETTINGS_PAGE_DIFFICULTY:
        return "ONE QUESTION, ASKED IN BOTH DIRECTIONS. NOTHING IS LOCKED.";
    case SETTINGS_PAGE_RECORDS:
        return "AN ASSIST RUN BANKS NO SCORE, NO TIME AND NO SHEETS.";
    case SETTINGS_PAGE_MAIN:
    case SETTINGS_PAGE_COUNT:
        break;
    }
    return "EVERY CHANGE IS KEPT WHEN THIS SHEET IS CLOSED.";
}

/*
 * The plate, and the one place that decides how tall a page is.
 *
 * Two heights are added up rather than one, and keeping them apart is the whole
 * fix: `fixed` is the headings, whose height is a rule plus a sentence and does
 * not shrink, and `flex` is the rows that carry a label and a sentence, which
 * do. The version this replaced scaled `flex` and then took the scale off the
 * total — headings included — so the plate came out shorter than what was drawn
 * on it and the last section of the sheet was printed over the footer and off
 * the bottom of the frame.
 */
SettingsLayout settings_page_layout(SettingsPage page, bool muted,
                                    float frame_h)
{
    int row_count = 0;
    const SettingRow *rows = settings_rows(page, &row_count);

    float fixed = 0.0f;
    float flex = 0.0f;
    for (int i = 0; i < row_count; ++i)
    {
        if (rows[i].kind == SETTING_ROW_BINDING)
            flex += SETTINGS_BIND_H;
        else if (rows[i].kind != SETTING_ROW_HEADING)
            flex += SETTINGS_VALUE_H;
        else if (rows[i].detail != NULL ||
                 (muted && settings_heading_governs_levels(rows, row_count, i)))
            fixed += SETTINGS_HEADING_DETAIL_H;
        else
            fixed += SETTINGS_HEADING_H;
    }

    SettingsLayout layout;
    layout.squeeze = 1.0f;
    layout.fits = true;

    float budget = frame_h - SETTINGS_FRAME_MARGIN - SETTINGS_ROWS_TOP -
                   SETTINGS_FOOTER_BAND;
    if (fixed + flex > budget && flex > 0.0f)
    {
        layout.squeeze = (budget - fixed) / flex;
        if (layout.squeeze < SETTINGS_SQUEEZE_MIN)
        {
            /* Past this a detail line touches the label under it, which is a
             * table too long for the frame rather than something to go on
             * shrinking around. The page is still drawn — a blank sheet would be
             * worse than a crowded one — and the suite is what refuses it. */
            layout.squeeze = SETTINGS_SQUEEZE_MIN;
            layout.fits = false;
        }
    }

    layout.value_h = SETTINGS_VALUE_H * layout.squeeze;
    layout.bind_h = SETTINGS_BIND_H * layout.squeeze;
    layout.rows_h = fixed + flex * layout.squeeze;
    layout.plate_h = SETTINGS_ROWS_TOP + layout.rows_h + SETTINGS_FOOTER_BAND;

    /*
     * And how many more rows the page could take, which is `fits` asked as a
     * number.
     *
     * A page fits while `fixed + flex * SETTINGS_SQUEEZE_MIN` is inside the
     * budget, so what one more value row costs is a squeezed row rather than a
     * whole one, and the answer is that difference divided out. The epsilon is
     * for the case where it comes out exact: a page with room for precisely two
     * more rows must not report one because the division landed on 1.9999998.
     *
     * The epsilon covers the other end too: `capacity - held` is nought or more
     * on any page that fits, so it can never floor to -1 on a rounding error.
     */
    const float per_row = SETTINGS_VALUE_H * SETTINGS_SQUEEZE_MIN;
    float held = flex / SETTINGS_VALUE_H;
    float capacity = (budget - fixed) / per_row;
    /* No clamp under this, and there must not be one: `fits` is exactly
     * `fixed + flex * SETTINGS_SQUEEZE_MIN <= budget`, which rearranges to
     * `capacity >= held`, so a page that fits cannot report a negative number.
     * A guard here would be a line nothing can execute standing where a reader
     * would take it for a case that occurs. */
    layout.spare_rows = (int)floorf(capacity - held + 1.0e-4f);
    return layout;
}

/*
 * The run of caps, laid out from the right margin inwards.
 *
 * The pad's pair ends on the margin and the keyboard's pair sits to its left,
 * which is the opposite of the way this was written and is the whole of the fix:
 * slot 0 is a key, the caret starts there, and the caret has to start at the
 * *left* end of a run it walks rightwards. See `SettingsCap`.
 */
int settings_bind_caps(float right, SettingsCap *out)
{
    if (out == NULL)
        return 0;

    const float cap_w =
        (float)KEYBIND_NAME_MAX * SETTINGS_GLYPH_W + SETTINGS_CAP_PAD;
    const float pad_cap_w =
        (float)PADBIND_NAME_MAX * SETTINGS_GLYPH_W + SETTINGS_CAP_PAD;
    const float run = (float)BIND_SLOTS * (cap_w + SETTINGS_CAP_GAP);
    const float pad_run = (float)BIND_SLOTS * (pad_cap_w + SETTINGS_CAP_GAP);

    float pad_left = right - pad_run + SETTINGS_CAP_GAP;
    float keys_left = pad_left - SETTINGS_CAP_GROUP_GAP - run + SETTINGS_CAP_GAP;

    for (int slot = 0; slot < BIND_TOTAL_SLOTS; ++slot)
    {
        bool pad = slot >= BIND_PAD_SLOT;
        int within = pad ? slot - BIND_PAD_SLOT : slot;
        out[slot].pad = pad;
        out[slot].w = pad ? pad_cap_w : cap_w;
        out[slot].x = (pad ? pad_left : keys_left) +
                      (float)within * (out[slot].w + SETTINGS_CAP_GAP);
    }
    return BIND_TOTAL_SLOTS;
}

bool settings_heading_governs_levels(const SettingRow *rows, int row_count,
                                     int index)
{
    if (rows == NULL || index < 0 || index >= row_count ||
        rows[index].kind != SETTING_ROW_HEADING)
    {
        return false;
    }
    for (int i = index + 1; i < row_count; ++i)
    {
        if (rows[i].kind == SETTING_ROW_HEADING)
            return false; /* the section ended without one */
        if (rows[i].kind == SETTING_ROW_SLIDER)
            return true;
    }
    return false;
}

BindAction settings_row_action(SettingId id)
{
    if (id < SETTING_BIND_FIRST)
        return BIND_COUNT;
    int action = (int)id - (int)SETTING_BIND_FIRST;
    return action < BIND_COUNT ? (BindAction)action : BIND_COUNT;
}

RunTallyRecord settings_row_readout(SettingId id)
{
    if (id < SETTING_READOUT_FIRST || id >= SETTING_BIND_FIRST)
        return RUN_TALLY_RECORD_COUNT;
    return (RunTallyRecord)((int)id - (int)SETTING_READOUT_FIRST);
}

const char *settings_row_label(const SettingRow *row)
{
    if (row == NULL)
        return "";
    if (row->label != NULL)
        return row->label;
    /* The only rows that leave it out are the readouts, whose names belong to
     * `run_tally.c` — see the note on `RECORD_ROWS`. Anything else with no label
     * is a row somebody forgot to name, and an empty string is what makes that
     * visible on the plate instead of dereferencing NULL in `draw_text`. */
    RunTallyRecord which = settings_row_readout(row->id);
    if (which == RUN_TALLY_RECORD_COUNT)
        return "";
    return run_tally_record_label(which);
}

const char *settings_row_armed_detail(SettingId id)
{
    switch (id)
    {
    case SETTING_RECORDS_RESET:
        return SETTINGS_RECORDS_ARMED_DETAIL;
    case SETTING_BINDINGS_RESET:
        return SETTINGS_BINDINGS_ARMED_DETAIL;
    default:
        return NULL;
    }
}

SettingsPage settings_row_opens(SettingId id)
{
    switch (id)
    {
    case SETTING_OPEN_CONTROLS:
        return SETTINGS_PAGE_CONTROLS;
    case SETTING_OPEN_DIFFICULTY:
        return SETTINGS_PAGE_DIFFICULTY;
    case SETTING_OPEN_RECORDS:
        return SETTINGS_PAGE_RECORDS;
    default:
        return SETTINGS_PAGE_COUNT;
    }
}

bool settings_row_is_reachable(SettingRowKind kind)
{
    /* A heading is a rule with a name on it and a readout is a number the game
     * is reporting: there is nothing on either for a change input to do, and a
     * caret parked on one is a caret the player has to press past. */
    return kind != SETTING_ROW_HEADING && kind != SETTING_ROW_READOUT;
}

bool settings_assist_any(const Settings *settings)
{
    if (settings == NULL)
        return false;
    return settings->assist.more_hearts || settings->assist.slower_guards ||
           settings->assist.infinite_lives;
}

void settings_defaults(Settings *settings)
{
    /*
     * Cleared whole before any field is named, so two `Settings` given the same
     * defaults compare equal byte for byte.
     *
     * Setting the fields one at a time leaves the struct's padding holding
     * whatever the caller's stack held — three bools at offsets 8, 9 and 10 are
     * followed by a byte nothing writes — so `memcmp` on two sheets that agree
     * about every setting could still report a difference. Nothing shipped read
     * it that way, because the file is written as text; the test that walks
     * every row and asks which field moved does, and a check that answers
     * differently depending on the stack under it is not a check.
     */
    memset(settings, 0, sizeof(*settings));

    /* Both levels start at the top, so a fresh install sounds exactly like the
     * mix the effects and the scores were written against: the sliders scale
     * that mix down and never up. */
    settings->music_volume = 100;
    settings->sfx_volume = 100;
    settings->fullscreen = false;
    settings->crt_filter = true;
    /* Off, because the shake and the strobes are how the game was written and
     * a comfort switch that is on by default is not a choice anybody made. */
    settings->reduced_motion = false;
    keybind_defaults(&settings->bindings);
    settings->assist.more_hearts = false;
    settings->assist.slower_guards = false;
    settings->assist.infinite_lives = false;
    settings->challenge.veteran = false;
}

int settings_first_row(SettingsPage page)
{
    int count = 0;
    const SettingRow *rows = settings_rows(page, &count);
    for (int i = 0; i < count; ++i)
    {
        if (settings_row_is_reachable(rows[i].kind))
            return i;
    }
    return 0;
}

int settings_move_cursor(SettingsPage page, int cursor, int delta)
{
    if (delta == 0)
        return cursor;

    int count = 0;
    const SettingRow *rows = settings_rows(page, &count);
    if (count <= 0)
        return cursor;

    int step = delta > 0 ? 1 : -1;
    int at = cursor;
    /* Walk at most one full lap. A sheet of nothing but headings would
     * otherwise spin here forever, and a table is an easy thing to edit into
     * that state by accident. */
    for (int guard = 0; guard < count; ++guard)
    {
        at = (at + step + count) % count;
        if (settings_row_is_reachable(rows[at].kind))
            return at;
    }
    return cursor;
}

static int clamp_percent(int value)
{
    if (value < 0)
        return 0;
    if (value > 100)
        return 100;
    return value;
}

static bool step_volume(int *value, int delta)
{
    int next = clamp_percent(*value + delta * SETTING_VOLUME_STEP);
    if (next == *value)
        return false;
    *value = next;
    return true;
}

static bool flip(bool *value)
{
    *value = !*value;
    return true;
}

bool settings_adjust(Settings *settings, SettingId id, int delta)
{
    switch (id)
    {
    case SETTING_MUSIC_VOLUME:
        return step_volume(&settings->music_volume, delta);
    case SETTING_SFX_VOLUME:
        return step_volume(&settings->sfx_volume, delta);
    case SETTING_FULLSCREEN:
        return flip(&settings->fullscreen);
    case SETTING_CRT_FILTER:
        return flip(&settings->crt_filter);
    case SETTING_REDUCED_MOTION:
        return flip(&settings->reduced_motion);
    case SETTING_MORE_HEARTS:
        return flip(&settings->assist.more_hearts);
    case SETTING_SLOWER_GUARDS:
        return flip(&settings->assist.slower_guards);
    case SETTING_INFINITE_LIVES:
        return flip(&settings->assist.infinite_lives);
    case SETTING_VETERAN:
        return flip(&settings->challenge.veteran);
    /* None of these hold a value that "less" and "more" mean anything to. A
     * binding is taken rather than adjusted — left and right walk its two
     * slots, which is the shell's cursor and not a value at all — and the rest
     * are pressed: two of them open another page and two do something once.
     * Returning false is what keeps the change inputs from clicking at a row
     * nothing happened to. */
    case SETTING_OPEN_CONTROLS:
    case SETTING_OPEN_DIFFICULTY:
    case SETTING_OPEN_RECORDS:
    case SETTING_BINDINGS_RESET:
    case SETTING_RECORDS_RESET:
    case SETTING_READOUT_FIRST:
    case SETTING_BIND_FIRST:
    case SETTING_NONE:
        break;
    }
    return false;
}

int settings_value_percent(const Settings *settings, SettingId id)
{
    switch (id)
    {
    case SETTING_MUSIC_VOLUME:
        return settings->music_volume;
    case SETTING_SFX_VOLUME:
        return settings->sfx_volume;
    default:
        return 0;
    }
}

bool settings_value_bool(const Settings *settings, SettingId id)
{
    switch (id)
    {
    case SETTING_FULLSCREEN:
        return settings->fullscreen;
    case SETTING_CRT_FILTER:
        return settings->crt_filter;
    case SETTING_REDUCED_MOTION:
        return settings->reduced_motion;
    case SETTING_MORE_HEARTS:
        return settings->assist.more_hearts;
    case SETTING_SLOWER_GUARDS:
        return settings->assist.slower_guards;
    case SETTING_INFINITE_LIVES:
        return settings->assist.infinite_lives;
    case SETTING_VETERAN:
        return settings->challenge.veteran;
    default:
        return false;
    }
}

/* ---- The file ------------------------------------------------------- */

/*
 * `key value` a line, because the one thing worth more than a compact format
 * here is that a player who opens the file can see what is in it and fix it.
 * The keys are spelled out once, in the writer and the reader together, so a
 * renamed field cannot be written under one name and looked for under another.
 */
#define KEY_MUSIC "music"
#define KEY_SFX "sfx"
#define KEY_FULLSCREEN "fullscreen"
#define KEY_CRT "crt"
#define KEY_MOTION "reduced_motion"
#define KEY_HEARTS "assist_hearts"
#define KEY_GUARDS "assist_guards"
#define KEY_LIVES "assist_lives"
#define KEY_VETERAN "challenge_veteran"

size_t settings_serialize(const Settings *settings, char *out, size_t cap)
{
    if (out == NULL || cap == 0)
        return 0;

    int written = snprintf(
        out, cap,
        "# Chuck settings. Delete this file to go back to the defaults.\n"
        KEY_MUSIC " %d\n"
        KEY_SFX " %d\n"
        KEY_FULLSCREEN " %d\n"
        KEY_CRT " %d\n"
        KEY_MOTION " %d\n"
        KEY_HEARTS " %d\n"
        KEY_GUARDS " %d\n"
        KEY_LIVES " %d\n"
        KEY_VETERAN " %d\n",
        clamp_percent(settings->music_volume),
        clamp_percent(settings->sfx_volume),
        settings->fullscreen ? 1 : 0,
        settings->crt_filter ? 1 : 0,
        settings->reduced_motion ? 1 : 0,
        settings->assist.more_hearts ? 1 : 0,
        settings->assist.slower_guards ? 1 : 0,
        settings->assist.infinite_lives ? 1 : 0,
        settings->challenge.veteran ? 1 : 0);

    if (written < 0)
    {
        out[0] = '\0';
        return 0;
    }
    if ((size_t)written >= cap)
        return cap - 1;

    /*
     * The bindings, by name rather than by number, one line an action: a
     * scancode in a text file is a number nobody can check and a key nobody can
     * fix by hand, and the whole reason this file is `key value` lines is that
     * a player who opens it can see what is in it. An empty slot is written as
     * `-`, so a line always has both fields and a missing one is damage rather
     * than a shorthand.
     */
    size_t at = (size_t)written;
    for (int action = 0; action < BIND_COUNT; ++action)
    {
        const int *slots = settings->bindings.keys[action];
        const char *first = keybind_key_name(slots[0]);
        const char *second = keybind_key_name(slots[1]);
        int line = snprintf(out + at, cap - at, "%s %s %s\n",
                            keybind_action_file_key((BindAction)action),
                            first[0] != '\0' ? first : "-",
                            second[0] != '\0' ? second : "-");
        if (line < 0 || (size_t)line >= cap - at)
        {
            /* Out of room: what is already written is a valid file, so it is
             * kept and terminated rather than thrown away. A truncated
             * settings file must still load. */
            out[at] = '\0';
            return at;
        }
        at += (size_t)line;
    }

    /*
     * The pad, on lines of its own rather than as two more fields on the
     * keyboard's.
     *
     * Separate lines mean a settings file written by the build before pads
     * were bindable still loads exactly as it did — its nine `bind_*` rows
     * apply and the pad keeps its defaults — and a file written by this build
     * loads on that one, which ignores keys it does not know. Sharing a line
     * would have made the pad half a change to the *format* of a line that
     * already exists, and an older build reading four names where it expects
     * two treats the whole row as damage and drops a binding the player set.
     *
     * Positional names, never letters: see the note on `keybind_pad_face_index`
     * for why a file that said `A` would be a file that means a different
     * button on a Switch pad than on an Xbox one.
     */
    for (int action = 0; action < BIND_COUNT; ++action)
    {
        const int *slots = settings->bindings.pad[action];
        const char *first = keybind_pad_file_name(slots[0]);
        const char *second = keybind_pad_file_name(slots[1]);
        int line = snprintf(out + at, cap - at, "%s %s %s\n",
                            keybind_action_pad_file_key((BindAction)action),
                            first[0] != '\0' ? first : "-",
                            second[0] != '\0' ? second : "-");
        if (line < 0 || (size_t)line >= cap - at)
        {
            out[at] = '\0';
            return at;
        }
        at += (size_t)line;
    }
    return at;
}

/*
 * One binding line's value: `NAME NAME`, either of which may be `-`.
 *
 * Both slots are read before either is written, for the reason every other
 * reader in this file leaves a field alone when it cannot parse it: a line with
 * one good name and one this build has never heard of is damage, and half
 * applying it would leave the action holding a key the player never chose
 * beside one they did.
 */
static bool read_binding(const char *value, int *first, int *second)
{
    int slot[BIND_SLOTS] = {KEYBIND_NONE, KEYBIND_NONE};
    for (int i = 0; i < BIND_SLOTS; ++i)
    {
        while (*value == ' ' || *value == '\t')
            ++value;
        const char *start = value;
        while (*value != '\0' && *value != ' ' && *value != '\t')
            ++value;
        size_t len = (size_t)(value - start);
        if (len == 0)
            return false;
        if (len == 1 && start[0] == '-')
            continue;
        slot[i] = keybind_key_from_name(start, len);
        if (slot[i] == KEYBIND_NONE)
            return false;
    }
    *first = slot[0];
    *second = slot[1];
    return true;
}

/* The pad's line, read exactly as the keyboard's is above and refused whole
 * for the same reason: half a row applied is an action holding a button the
 * player never chose beside one they did. */
static bool read_pad_binding(const char *value, int *first, int *second)
{
    int slot[BIND_SLOTS] = {PADBIND_NONE, PADBIND_NONE};
    for (int i = 0; i < BIND_SLOTS; ++i)
    {
        while (*value == ' ' || *value == '\t')
            ++value;
        const char *start = value;
        while (*value != '\0' && *value != ' ' && *value != '\t')
            ++value;
        size_t len = (size_t)(value - start);
        if (len == 0)
            return false;
        if (len == 1 && start[0] == '-')
            continue;
        slot[i] = keybind_pad_from_file_name(start, len);
        if (slot[i] == PADBIND_NONE)
            return false;
    }
    *first = slot[0];
    *second = slot[1];
    return true;
}

/* True when `line` opens with `key` followed by a separator, and hands back
 * whatever follows it. Requiring the separator is what stops `music` from
 * matching a future `music_device`. */
static bool line_key_is(const char *line, const char *key, const char **value)
{
    size_t n = strlen(key);
    if (strncmp(line, key, n) != 0)
        return false;
    const char *rest = line + n;
    if (*rest != ' ' && *rest != '\t' && *rest != '=')
        return false;
    while (*rest == ' ' || *rest == '\t' || *rest == '=')
        ++rest;
    *value = rest;
    return true;
}

/*
 * A number, and whether there was one at all.
 *
 * `strtol` reads "no digits" as zero, which would make a half-written line the
 * one kind of damage that does change a setting: `crt ` left behind by an
 * interrupted write reads as a nought and turns the filter off. A key this
 * build recognises with a value it cannot read is treated exactly as a key it
 * does not recognise — the setting keeps whatever it already had.
 */
static bool read_int(const char *value, int *out)
{
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (end == value)
        return false;
    /* Clamped as a long before the cast, because a hand-edited 99999999999 is
     * out of range for an int and the conversion itself would be undefined. */
    if (parsed > INT_MAX)
        parsed = INT_MAX;
    if (parsed < INT_MIN)
        parsed = INT_MIN;
    *out = (int)parsed;
    return true;
}

static void apply_line(Settings *settings, const char *line)
{
    while (*line == ' ' || *line == '\t')
        ++line;
    if (*line == '\0' || *line == '#')
        return;

    const char *value = NULL;
    int number = 0;

    /* The bindings first and on their own, because they are the one kind of
     * line whose value is not a number. Matching one ends the line: the chain
     * below is a single `else if` ladder precisely so that no line can ever be
     * applied twice, and a `for` cannot be a rung of it. */
    for (int action = 0; action < BIND_COUNT; ++action)
    {
        /* The pad row is tried first, and it has to be: `line_key_is` matches
         * on a prefix, and while `pad_bind_left` cannot be mistaken for
         * `bind_left` in this direction, testing the shorter key first is the
         * habit that makes the next pair of keys a silent bug. */
        if (line_key_is(line, keybind_action_pad_file_key((BindAction)action),
                        &value))
        {
            int first = PADBIND_NONE;
            int second = PADBIND_NONE;
            if (read_pad_binding(value, &first, &second))
            {
                settings->bindings.pad[action][0] = first;
                settings->bindings.pad[action][1] = second;
            }
            return;
        }
        if (!line_key_is(line, keybind_action_file_key((BindAction)action),
                         &value))
        {
            continue;
        }
        int first = KEYBIND_NONE;
        int second = KEYBIND_NONE;
        if (read_binding(value, &first, &second))
        {
            settings->bindings.keys[action][0] = first;
            settings->bindings.keys[action][1] = second;
        }
        return;
    }

    if (line_key_is(line, KEY_MUSIC, &value))
    {
        if (read_int(value, &number))
            settings->music_volume = clamp_percent(number);
    }
    else if (line_key_is(line, KEY_SFX, &value))
    {
        if (read_int(value, &number))
            settings->sfx_volume = clamp_percent(number);
    }
    else if (line_key_is(line, KEY_FULLSCREEN, &value))
    {
        if (read_int(value, &number))
            settings->fullscreen = number != 0;
    }
    else if (line_key_is(line, KEY_CRT, &value))
    {
        if (read_int(value, &number))
            settings->crt_filter = number != 0;
    }
    else if (line_key_is(line, KEY_MOTION, &value))
    {
        if (read_int(value, &number))
            settings->reduced_motion = number != 0;
    }
    else if (line_key_is(line, KEY_HEARTS, &value))
    {
        if (read_int(value, &number))
            settings->assist.more_hearts = number != 0;
    }
    else if (line_key_is(line, KEY_GUARDS, &value))
    {
        if (read_int(value, &number))
            settings->assist.slower_guards = number != 0;
    }
    else if (line_key_is(line, KEY_LIVES, &value))
    {
        if (read_int(value, &number))
            settings->assist.infinite_lives = number != 0;
    }
    else if (line_key_is(line, KEY_VETERAN, &value))
    {
        if (read_int(value, &number))
            settings->challenge.veteran = number != 0;
    }
    /* Anything else is a line from a build that knew something this one does
     * not. Ignoring it is deliberate: a settings file must never be the reason
     * a game refuses to start. */
}

void settings_parse(Settings *settings, const char *text)
{
    if (text == NULL)
        return;

    /* Zeroed on the way in only to keep the static analyser from reading the
     * scan below as working on an uninitialised buffer; every line is
     * terminated before it is handed on regardless. */
    char line[128] = {0};
    size_t len = 0;
    for (const char *at = text;; ++at)
    {
        if (*at == '\n' || *at == '\r' || *at == '\0')
        {
            line[len] = '\0';
            if (len > 0)
                apply_line(settings, line);
            len = 0;
            if (*at == '\0')
                return;
            continue;
        }
        /* A line longer than the buffer is truncated rather than run off the
         * end of it; what survives either parses or is ignored. */
        if (len + 1 < sizeof(line))
            line[len++] = *at;
    }
}
