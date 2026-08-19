#!/bin/bash
#
# Run the game across every sector, headless, and fail if anything complains.
#
# This exists because of a hole in the safety net rather than because of a bug.
# `make sanitize` compiled the whole tree with ASan and UBSan — the renderers,
# the level art, the audio synth, the cutscenes, the manual — and then ran only
# `core_tests`, which links no SDL at all. The sanitized *game* was built and
# never started. CI could not have started it either: that job builds SDL with
# `-DSDL_X11=OFF -DSDL_WAYLAND=OFF`, so the binary had no window to open.
#
# What that left uncovered is more than half the tree by source size:
# `game_render.c`, `level_art.c`, `cutscene.c`, `render_figures.c`, `audio.c`,
# `intro.c`, `manual.c` and `chase_render.c`. Every one of them was
# sanitizer-*compiled* and never sanitizer-*executed*, under a target whose name
# is `sanitize`. That is the shape of failure this repository keeps finding on
# the floor — a check that passes precisely because it never reaches the thing
# it names — and it was the sanitizers themselves this time.
#
# The dummy video driver is what makes it a check rather than a no-op. SDL falls
# back to the software renderer, which really does rasterize: a sanitized sector
# burns most of a core while this runs, so the draw calls are being executed and
# not merely linked. SDL always builds the dummy driver, so this works on the
# CI job that has no real video backend at all.
#
# `--soak N` rather than a `kill` from here, because a killed process never
# reaches `SDL_AppQuit`: teardown is the half of the lifecycle a sanitizer is
# most likely to have something to say about. The game closes itself and this
# script reads the exit status.
set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
binary=${1:-$root/chuck}
seconds=${2:-2}

# **What this run is for, because there are two different questions here and one
# of them costs five minutes more than the other.**
#
# `full` is the coverage sweep this script was written to be: every screen the
# game can be put on, every sheet of the manual, every pose of an aftermath, and
# every timed sequence held for as long as the game's own constants say its beat
# lasts. That last part is where the minutes are — the drive, the ending and the
# roll of names are two of those minutes between them — and it is worth every one
# of them under a sanitizer, which is the job it was measured for.
#
# `smoke` is the other question: does *this* build, on *this* platform, start,
# load every map in the campaign, open every room and run the editor. It walks
# everything that holds still and holds no timed sequence at all, because a beat
# of `cutscene.c` executed on arm64 is the same beat executed an hour earlier
# under ASan on the other job — the second run of it is a platform check wearing
# a coverage sweep's clock. What it skips it names on the way out, because a mode
# that quietly covers less is this repository's oldest defect.
mode=${SOAK_MODE:-full}
case $mode in
    full | smoke) ;;
    *)
        echo "soak: SOAK_MODE is 'full' or 'smoke', not '$mode'" >&2
        exit 1
        ;;
esac

# Resolved to an absolute path before anything runs it, and that is not tidiness.
# The Makefile's own `TARGET` is the bare word `chuck`, so this arrived as a name
# with no slash in it — which bash looks for on `PATH` rather than in the
# directory it plainly means. `-x chuck` was true, the guard below passed, and
# every one of the eighteen runs then failed with `command not found`: a script
# that reported the build broken because of how it had been handed the build.
case $binary in
    /*) ;;
    *) binary=$(cd "$(dirname "$binary")" && pwd)/$(basename "$binary") ;;
esac

if [ ! -x "$binary" ]; then
    echo "soak: no executable at $binary" >&2
    exit 1
fi

# The campaign's length is counted rather than written down, for the reason
# everything else about the campaign's length is: a sector added to levels/ is a
# sector this sweep has to walk, and a literal 17 here would have gone stale the
# same day every other literal 17 in this tree did.
sectors=$(find "$root/levels" -maxdepth 1 -name 'level*.txt' | wc -l | tr -d ' ')
# And the manual's length, read out of the header that defines it for the same
# reason. The sheaf is one `--screen` name standing for ten drawings, and nothing
# turns a sheet but a hand: without walking them the sweep covered the first
# illustration and left nine of them sanitizer-compiled and never executed.
sheets=$(sed -n 's/^#define MANUAL_PAGE_COUNT \([0-9]*\).*/\1/p' \
    "$root/src/manual_pages.h")
if [ -z "$sheets" ]; then
    echo "soak: could not read MANUAL_PAGE_COUNT from src/manual_pages.h" >&2
    exit 1
fi
if [ "$sectors" -lt 1 ]; then
    echo "soak: found no levels/level*.txt to walk" >&2
    exit 1
fi

# A full template rather than `mktemp -t chuck-soak`, and the difference is a
# platform: BSD's `-t` takes a *prefix* and appends the random part itself, so
# the short form worked on the machine this was written on and on the macOS CI
# job. GNU coreutils reads `-t` as a deprecated flag whose argument is still a
# template and refuses one with no X's in it — `too few X's in template` — so
# `$log` came back empty on the Linux sanitizer job, every redirect into it
# failed, and all eighteen runs were reported as the game exiting 1. A script
# whose whole purpose is to make a silent failure loud spent that run blaming
# the build for its own temporary file.
#
# Checked as well as spelled portably: an unwritable TMPDIR is the one other
# way this comes back empty, and it must say so here rather than eighteen
# confusing lines further down.
log=$(mktemp "${TMPDIR:-/tmp}/chuck-soak.XXXXXX") || log=""
if [ -z "$log" ]; then
    echo "soak: could not create a temporary file in ${TMPDIR:-/tmp}" >&2
    exit 1
fi
trap 'rm -f "$log"' EXIT

# ASan's leak detector stays off. SDL and the platform's own graphics stack
# leave allocations behind at exit that this game never owned, so leaks here
# would be somebody else's and would fail the build for it. Everything ASan and
# UBSan are actually being asked about — bad reads and writes, overflow,
# alignment, the undefined arithmetic in a renderer — is unaffected by it.
export ASAN_OPTIONS="${ASAN_OPTIONS:-}:detect_leaks=0"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-}:print_stacktrace=1:halt_on_error=1"
export SDL_VIDEODRIVER=dummy
export SDL_AUDIODRIVER=dummy

failures=0

# Anything a sanitizer says, plus the game's own two ways of reporting that a
# map did not load. A soak that boots the title screen instead of the sector it
# was pointed at is a pass by exit status and a failure by intent, which is
# exactly the class of quiet miss this whole script is about.
complaints='runtime error|AddressSanitizer|LeakSanitizer|SUMMARY: .*Sanitizer|ERROR: |Could not |is outside the campaign'

# Read one `#define NAME <number>` out of a header, the way the manual's sheet
# count is read above and for the same reason: the screens below are held for as
# long as the game says they last, so a beat that gets longer is walked to its
# end by having been lengthened rather than by somebody remembering this file.
# The trailing `f` of a float literal is dropped; `--soak` parses a float.
define_of()
{
    local name=$1 header=$2 value
    value=$(sed -n "s/^#define $name  *\([0-9][0-9.]*\)f\{0,1\}.*/\1/p" \
        "$root/src/$header" | head -1)
    if [ -z "$value" ]; then
        echo "soak: could not read $name from src/$header" >&2
        exit 1
    fi
    printf '%s' "$value"
}

# How long a screen has to be held for all of it to be drawn.
#
# **The ordinary two seconds silently truncated every timed sequence, and the
# sweep called it covered.** A `--screen` name puts the game on the first frame
# of a beat; nothing but the clock advances it, so a two-second hold on a
# twenty-seven-second outro rasterized the opening beat and stopped. Measured
# with a coverage build, that cost fifteen of `cutscene.c`'s drawing functions
# and three of `chase_render.c`'s — among them `draw_helicopter`, sixty-three
# lines of the ending the whole campaign is played for, and `draw_reunion_pair`,
# which is the payoff itself. Every one of them went from nought to fully
# executed when the hold was long enough to reach it. The screens were reachable
# and the budget was not reaching them, which is this script's own recurring
# defect — coverage reported rather than had — one level below where it was last
# found.
#
# The spans are the game's own constants rather than numbers chosen here, and the
# ordinary budget is added on top as the margin: a beat is drawn *up to* its
# duration, so a hold of exactly that length can miss its last frame.
screen_seconds()
{
    local span
    case $1 in
        abduction) span=$(define_of ABDUCTION_CUTSCENE_DURATION cutscene.h) ;;
        opening) span=$(define_of OPENING_CUTSCENE_DURATION cutscene.h) ;;
        report) span=$(define_of LEVEL_TRANSITION_DURATION cutscene.h) ;;
        outro) span=$(define_of OUTRO_CUTSCENE_DURATION cutscene.h) ;;
        credits) span=$(define_of CREDITS_MAX_DURATION credits.h) ;;
        continue) span=$(define_of CONTINUE_COUNTDOWN_TIME game_config.h) ;;
        gameover) span=$(define_of GAME_OVER_DISPLAY_TIME game_config.h) ;;
        # The drive is two beats end to end, and the second is the one with the
        # cordon cars and the wreck in it.
        chase)
            span=$(awk \
                -v a="$(define_of CHASE_DEPARTURE_DURATION game_config.h)" \
                -v b="$(define_of CHASE_PURSUIT_DURATION game_config.h)" \
                'BEGIN { print a + b }')
            ;;
        # The rest are sheets and cards: they hold still until a hand moves
        # them, so the ordinary budget draws all there is of them.
        *)
            printf '%s' "$seconds"
            return
            ;;
    esac
    awk -v s="$span" -v m="$seconds" 'BEGIN { printf "%.1f", s + m }'
}

# Whether a screen's budget comes from a duration the game defines, asked of
# `screen_seconds` itself rather than of a second list of names beside it: what
# makes a screen expensive and what makes it worth a sanitizer's time are the
# same property, so there is one place that knows which screens have it. A list
# here would be the copy that goes stale the day a beat gains a clock.
screen_is_timed()
{
    [ "$(screen_seconds "$1")" != "$seconds" ]
}

soak_one()
{
    local what=$1
    local budget=$2
    shift 2

    # The status is captured rather than read out of `$?` after an `if`, which
    # is where the first version of this got it wrong: `if ! cmd; then` has
    # already reset `$?` by the time the branch runs, so every early exit was
    # reported as "exited 0" — a failure message naming success, in the script
    # whose whole job is to make a silent failure loud.
    local status=0
    "$binary" --soak "$budget" "$@" >"$log" 2>&1 || status=$?
    if [ "$status" -ne 0 ]; then
        echo "soak: $what exited $status before its budget ran out"
        sed 's/^/      /' "$log"
        failures=$((failures + 1))
        return
    fi
    if grep -qE "$complaints" "$log"; then
        echo "soak: $what had something to say"
        grep -E "$complaints" "$log" | sed 's/^/      /'
        failures=$((failures + 1))
        return
    fi
    # The switch has to have been honoured, or a build that silently ignored it
    # would pass this sweep by drawing one frame and exiting.
    if ! grep -q "Soak finished" "$log"; then
        echo "soak: $what never reported finishing; --soak was not honoured"
        sed 's/^/      /' "$log"
        failures=$((failures + 1))
        return
    fi
    echo "soak: $what ok"
}

# The title screen first: it is the only entry point that reaches `intro.c` and
# the attract music, neither of which any `--level` run draws.
#
# **It reaches nothing beyond that, and this comment used to say it did.** The
# sentence here read "the title screen ... the prologue's cutscenes and the
# attract music", and the middle third was never true: `STATE_INTRO` advances on
# `game->input.confirm`, every line that sets that flag is inside an SDL event
# handler, and a headless run receives no events. So the sweep sat on the first
# screen for its whole budget while its own header claimed it had walked the
# prologue — a check reporting coverage it did not have, which is the exact
# failure this script was written to end, one floor up from where it ended it.
# Forty seconds of `--soak` proves it: the process reports finishing without
# ever having left the title screen.
#
# `--screen NAME` is the answer, and the list below is the rest of the game:
# the prologue's three beats, the three sheets a player opens, the four cards a
# sector or a run ends on, the ending and the roll after it, the restrooms that no
# `--level` run enters on its own, and the aftermath of a fight. Between them they
# are `chase_render.c` entire, most of `cutscene.c`, `manual.c`, and the settings,
# pause, continue and game-over halves of `game_render.c` — every one of which
# was sanitizer-compiled and never sanitizer-executed.
#
# A switch rather than synthesised keypresses, for the reason `--soak` is a
# switch rather than a `kill`: a screen reached by three fake button presses is
# a screen whose coverage breaks the day a menu gains a row, and what would be
# under test is the event handlers rather than the renderers.
#
# **A name is not a budget and it is not a page**, which is what a coverage build
# of this sweep found once somebody counted instead of assuming. Three of these
# names stand for more than one drawing and are walked by `--page` below; every
# timed sequence needed a hold as long as it lasts (`screen_seconds`); one of them
# drew the wrong card outright (`continue`, refused by
# `campaign_begin_continue` on a run with lives still in it); and the sector after
# a fight — the bodies, the opened patch, the alarm, the crawl, the bazooka — is
# reached by no run that presses nothing, which is what `aftermath` stages.
soak_one "title screen" "$seconds"

for sector in $(seq 1 "$sectors"); do
    soak_one "sector $sector" "$seconds" --level "$sector"
done

screens=(abduction chase opening manual settings pause report cleared
         continue gameover outro credits restroom aftermath)
walked=0
skipped=""
for screen in "${screens[@]}"; do
    if [ "$mode" = smoke ] && screen_is_timed "$screen"; then
        skipped="$skipped $screen"
        continue
    fi
    soak_one "screen $screen" "$(screen_seconds "$screen")" --screen "$screen"
    walked=$((walked + 1))
done

# Every sheet of the manual, because the name above only reaches the first one.
# `--page` is what makes the other nine drawings executable by this sweep; the
# count comes off the header, so a new sheet is walked by having been added.
for sheet in $(seq 1 "$sheets"); do
    soak_one "manual sheet $sheet" "$seconds" --screen manual --page "$sheet"
done

# Both halves of the options sheet, because the name above only reaches the
# first. The controls page is where every key cap and pad cap on the sheet is
# drawn (`draw_setting_keys`), and it was reached by no run: the same "one screen
# name, more than one drawing" shape as the manual's sheaf. The count comes off
# the header for the same reason the sheaf's does.
pages=$(sed -n 's/^#define SETTINGS_PAGE_COUNT \([0-9]*\).*/\1/p' \
    "$root/src/settings.h")
if [ -z "$pages" ]; then
    # An enum rather than a #define, which is what it is today: the pages are
    # `SETTINGS_PAGE_MAIN` and `SETTINGS_PAGE_CONTROLS`, counted here rather than
    # written down so a third one is walked by having been added.
    pages=$(sed -n 's/^ *SETTINGS_PAGE_\([A-Z_]*\),.*/\1/p' \
        "$root/src/settings.h" | grep -cv '^COUNT$')
fi
if [ "$pages" -lt 1 ]; then
    echo "soak: could not count the options sheet's pages in src/settings.h" >&2
    exit 1
fi
for page in $(seq 1 "$pages"); do
    soak_one "options page $page" "$seconds" --screen settings --page "$page"
done

# The four poses of a sector after it went wrong. `draw_player` answers hacking
# first and crawling second, so one frame holds one of them; between the four
# pages this reaches the bodies, the opened patch, the alarm lighting, the
# emitting half of the particle system and both the flat and the vertical bazooka.
# See `soak_stage_aftermath` in src/game.c for why the state is staged rather
# than played.
aftermath_poses=(1 2 3 4 5)
poses=0
for pose in "${aftermath_poses[@]}"; do
    soak_one "aftermath pose $pose" "$seconds" \
        --screen aftermath --page "$pose"
    poses=$((poses + 1))
done

# And the same poses again on the floor with trunking on it, because one of them
# is staged *inside* a duct there and nothing else in the game draws that.
#
# `render_duct_fronts` lays a shaft's louvres back over the man crawling behind
# them — the pass that stops a crawl being drawn outside the wall it went into —
# and the only world that reaches it is a world with Chuck in trunking. A
# headless run presses nothing, so no sweep has ever crawled into one, and the
# poses above are staged on whichever sector is the first with a `%` and a dog on
# it, which need not be a sector with trunking and today is not. Same shape as the
# restrooms below: one screen name, and which drawing it reaches decided by the
# map behind it.
#
# All five poses rather than the crawl alone, because which page is the crawl is
# `soak_stage_aftermath`'s business and a number here would be that fact written
# down twice. The sector is grepped out of the maps for the reason the facade
# below it is: a `=` painted into a new sector is a shaft this sweep has to walk.
ducts=""
for sector in $(seq 1 "$sectors"); do
    if grep -E '^[#. ]' "$root/levels/level$sector.txt" | grep -q '='; then
        ducts=$sector
        break
    fi
done
if [ -n "$ducts" ]; then
    for pose in "${aftermath_poses[@]}"; do
        soak_one "aftermath pose $pose in the ducts (sector $ducts)" "$seconds" \
            --screen aftermath --level "$ducts" --page "$pose"
        poses=$((poses + 1))
    done
else
    echo "soak: found no map with a '=' in it to stage a duct aftermath on" >&2
    failures=$((failures + 1))
fi

# And the same for a wall, which has none of an interior's pieces and two of its
# own: a thrown object in the air and a bird crossing. Both spawn on a timer, so
# whether a two-second sector run caught one was luck rather than coverage. The
# climb is the first `MODE FACADE` map, found rather than named.
facade=$(grep -l '^MODE FACADE' "$root"/levels/level*.txt 2>/dev/null |
    sed -n 's/.*level\([0-9]*\)\.txt/\1/p' | sort -n | head -1)
if [ -n "$facade" ]; then
    soak_one "aftermath on the wall (sector $facade)" "$seconds" \
        --screen aftermath --level "$facade"
    poses=$((poses + 1))
else
    echo "soak: found no MODE FACADE map to stage a wall aftermath on" >&2
    failures=$((failures + 1))
fi

# Every restroom, because `--screen restroom` above only reaches sector 1's.
#
# The same shape as the sheaf and it went unnoticed for as long: one screen name
# standing for four maps. `level_theme_sublevel` picks the room off the sector's
# `THEME`, so the sweep drew `restroom_lobby` and nothing else — and the toilet
# prop `q` is in the plant's, the archive's and the penthouse's rooms and in no
# other map in the game, which made `draw_restroom_toilet` a drawing function
# nothing in the tree ever executed.
#
# The sectors are grepped out of the maps rather than listed, for the reason the
# campaign's length is counted rather than written down: a `U` painted into a new
# sector is a room this sweep has to open, and a list here would be the copy that
# went stale. The body of a map is what is searched — a bare `U` in a metadata
# line such as `THEME PENTHOUSE` is not a door.
restrooms=0
for sector in $(seq 1 "$sectors"); do
    map=$root/levels/level$sector.txt
    if grep -E '^[#. ]' "$map" | grep -q 'U'; then
        soak_one "restroom off sector $sector" "$seconds" \
            --screen restroom --level "$sector"
        restrooms=$((restrooms + 1))
    fi
done
if [ "$restrooms" -lt 1 ]; then
    echo "soak: found no sector with a 'U' in it to open a restroom off" >&2
    exit 1
fi

# The editor, which was the last binary in the tree nothing ran.
#
# `make sanitize` builds `all test soak`, and `all` is the game — so
# `editor_app.c`, `editor_render.c` and `editor_ui.c`, ninety kilobytes of SDL
# between them, were not merely unexecuted under the sanitizers, they were never
# compiled under them. The macOS CI job builds the editor and then does nothing
# with it. `editor_validate.c` and `editor_doc.c` *are* held by the suite, and
# that is precisely what made the hole hard to see: the parts of the editor with
# a check on them are the parts that link no SDL.
#
# Two runs, because the editor has two ways of starting and they take different
# paths through `SDL_AppInit`: opening a map named on the command line, and
# opening whatever the repository's first map is. The second also exercises
# `ed_scan_files`, which the first walks past.
#
# Skipped rather than failed when there is no editor beside the binary, because
# `make soak` on its own is a smoke test of whatever happens to be built and
# `make editor` is a separate target. `make sanitize` builds both, so the sweep
# it runs always covers it.
soak_editor()
{
    local what=$1
    shift

    local status=0
    "$editor_binary" --soak "$seconds" "$@" >"$log" 2>&1 || status=$?
    if [ "$status" -ne 0 ]; then
        echo "soak: $what exited $status before its budget ran out"
        sed 's/^/      /' "$log"
        failures=$((failures + 1))
        return
    fi
    if grep -qE "$complaints" "$log"; then
        echo "soak: $what had something to say"
        grep -E "$complaints" "$log" | sed 's/^/      /'
        failures=$((failures + 1))
        return
    fi
    if ! grep -q "Soak finished" "$log"; then
        echo "soak: $what never reported finishing; --soak was not honoured"
        sed 's/^/      /' "$log"
        failures=$((failures + 1))
        return
    fi
    echo "soak: $what ok"
}

# `SOAK_EDITOR` rather than a name guessed beside the game, because the
# sanitized build does not put the two in the same place: the game is
# `build/chuck-sanitize` and the editor is built alongside it under a name of
# its own. The default is the ordinary layout, so `make soak` on its own still
# finds `./chuck-editor` next to `./chuck` without being told.
editor_binary=${SOAK_EDITOR:-$(dirname "$binary")/chuck-editor}
case $editor_binary in
    /*) ;;
    *) editor_binary=$(cd "$(dirname "$editor_binary")" && pwd)/$(basename "$editor_binary") ;;
esac
editors=0
if [ -x "$editor_binary" ]; then
    soak_editor "editor on level1.txt" "$root/levels/level1.txt"
    soak_editor "editor on the repository" "$root"
    editors=2
else
    echo "soak: no editor at $editor_binary; skipping it"
fi

if [ "$failures" -ne 0 ]; then
    echo "soak: $failures run(s) failed"
    exit 1
fi

# The budgets are no longer uniform, so the line does not claim they are: the
# timed sequences are held for as long as the game says they last and everything
# else for the ordinary budget. See `screen_seconds`.
#
# And it counts the screens it *walked* rather than the length of the list it was
# given, which are the same number in `full` and are not in `smoke`. Printing the
# list's length in both would be a summary reporting coverage it does not have,
# which is the sentence this whole file was written under.
#
# And the clause about the budgets is the mode's own, for the same reason: it read
# "their own full length for the timed sequences" in both, which in `smoke` is a
# summary describing the run beside it. The count was right and the sentence was
# wrong, which is the harder half of this to notice.
if [ -n "$skipped" ]; then
    held="${seconds}s each, and no timed sequence held at all"
else
    held="${seconds}s each for the sheets and cards, their own full length"
    held="$held for the timed sequences"
fi
echo "soak: title screen, $sectors sectors, $walked screens," \
     "$sheets manual sheets, $pages options pages, $poses aftermaths," \
     "$restrooms restrooms and $editors editor run(s) drew clean — $held"
if [ -n "$skipped" ]; then
    echo "soak: this was SOAK_MODE=smoke; it skipped$skipped." \
         "SOAK_MODE=full is the sweep those are covered by."
fi
