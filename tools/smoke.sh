#!/bin/sh
# Boot the game for real, against a dummy display, and see whether it survives.
#
# `make test` links no SDL, which is what makes it fast and portable and is also
# the reason it can never touch the renderers, the level art or the audio
# synthesis — about 24k of the tree's 56k lines, measured rather than pinned, so
# read them as a proportion and not to the digit. `make sanitize` builds that
# half under ASan/UBSan and then runs only the core suite, so the instrumented
# binary it just produced was never executed at all.
#
# This is what executes it: the title screen, then every campaign sector in
# turn, then every screen that is only otherwise reached by playing — each for
# a few seconds of real frames. SDL's dummy video and audio drivers mean it
# needs no display and no sound card, so it runs the same on a laptop and in CI.
#
# That last group is why `--scene` exists, and it was added because it was
# missing: this script used to boot the title screen and the fifteen sectors
# and press no key, so the two prologue cutscenes, the drive, the manual, the
# options sheet, the report between sectors, the outro and the roll of names
# were executed by nothing in the tree at all. The first run that reached them
# found undefined behaviour in the credits skyline, going off in every frame of
# the one screen a finished campaign always ends on.
#
# It asserts nothing about what was drawn. What it catches is the class the
# core suite structurally cannot: a bad index in a draw loop, a read past a
# theme table, an uninitialised value in the audio synth — anything ASan or
# UBSan will say out loud the moment the code is actually run. Any output from
# the binary is treated as failure, because a clean run of this game is silent.
#
# **A dwell is not a coverage claim, and that is the lesson this file learned
# second.** Every scene used to be given the same few seconds, which is right
# for a screen that is a *still* — a sector, the pause sheet, the options — and
# wrong for every screen that is a *clock*. The outro runs twenty-seven seconds,
# the roll of names thirty-odd, the drive over a minute; three seconds of each
# executed the frame they open on and reported the whole screen as covered. So
# the outro's closing card, the credits' own last card and the drive's arrival
# were in exactly the state the credits skyline was in before this file existed:
# drawn by code nothing in the tree ever ran. Each of those scenes now dwells
# for its own length, read out of the header that owns the number rather than
# copied here — the rule CI already keeps for the pinned SDL version, because
# two hand-kept copies of a number are two numbers.
#
# **And the manual is neither**, which is the third shape and the one that needs
# a switch rather than a clock: it is eight sheets, each with an illustration of
# its own, and a sheet is only ever turned by a hand. A run that presses no keys
# draws the first and none of the other seven — some six hundred lines of
# drawing, in a file `make test` cannot link, whose *words* are all measured by
# the suite. `--page N` is what reaches them, and all eight are run below.

set -eu

BINARY=${1:?usage: smoke.sh PATH_TO_BINARY}
SECONDS_PER_SCENE=${SECONDS_PER_SCENE:-3}
SECTORS=${SECTORS:-15}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ ! -x "$BINARY" ]; then
    echo "smoke: $BINARY is not executable — run make sanitize first" >&2
    exit 1
fi

# A number out of a header, by the name it is `#define`d under, assigned to a
# variable of this script's own. Anything this run waits for is a length the
# game itself decides, so it is read from where the game decides it; a copy kept
# here would be right on the day it was typed and wrong from the first tuning
# pass after it.
#
# It is a *setter* rather than something to call inside `$(...)`, and that is
# the whole reason it is shaped this way: a failure inside a command
# substitution leaves only its subshell, so a nested `$(define_of ...)` that
# could not find its constant would hand `awk` an empty string and this run
# would quietly fall back to the base dwell — which is precisely the silent
# under-coverage the rest of this file exists to stop.
define_of() {
    _value=$(sed -n "s/^#define $2  *\([0-9][0-9.]*\)f\{0,1\}.*\$/\1/p" \
        "$root/$3" | head -1)
    if [ -z "$_value" ]; then
        echo "smoke: could not read $2 from $3" >&2
        exit 1
    fi
    eval "$1=\$_value"
}

define_of abduction_len ABDUCTION_CUTSCENE_DURATION src/cutscene.h
define_of arrival_len OPENING_CUTSCENE_DURATION src/cutscene.h
define_of report_len LEVEL_TRANSITION_DURATION src/cutscene.h
define_of outro_len OUTRO_CUTSCENE_DURATION src/cutscene.h
# The roll's own length is a property of the table rather than a constant, so
# the header states a ceiling instead and `test_credits_fit_the_frame` holds the
# table under it. Waiting out the ceiling is what makes the closing card a thing
# this run reaches however many names the roll grows.
define_of credits_len CREDITS_MAX_DURATION src/credits.h
# The drive is the long one, and it is long for a reason worth writing down: an
# unsteered car still coasts to the cruise speed, so it drives the whole
# pursuit — and crashes its way through it, and every crash costs a beat of wall
# clock that the pursuit clock does not count. Departure, pursuit and arrival
# are the three phase lengths; the allowance is for those crashes. Measured
# across forty idle seeds the drive reached the building between 66 and 73
# seconds, so twelve failed beats is margin rather than a guess.
define_of chase_departure CHASE_DEPARTURE_DURATION src/game_config.h
define_of chase_pursuit CHASE_PURSUIT_DURATION src/game_config.h
define_of chase_arrival CHASE_ARRIVAL_DURATION src/game_config.h
define_of chase_failed CHASE_FAILED_DURATION src/game_config.h

# The dwell for one scene: the base, or the scene's own length plus a beat to
# land on, whichever is longer. `SECONDS_PER_SCENE` therefore still says how
# long to sit on a still, and cannot cut a clock short.
seconds() {
    awk -v a="$1" -v b="$SECONDS_PER_SCENE" 'BEGIN { print (a + 2 > b) ? a + 2 : b }'
}

ABDUCTION_SECONDS=$(seconds "$abduction_len")
ARRIVAL_SECONDS=$(seconds "$arrival_len")
REPORT_SECONDS=$(seconds "$report_len")
OUTRO_SECONDS=$(seconds "$outro_len")
CREDITS_SECONDS=$(seconds "$credits_len")
DRIVE_SECONDS=$(seconds "$(awk \
    -v departure="$chase_departure" -v pursuit="$chase_pursuit" \
    -v arrival="$chase_arrival" -v failed="$chase_failed" \
    'BEGIN { print departure + pursuit + arrival + 12 * failed }')")

export SDL_VIDEODRIVER=dummy
export SDL_AUDIODRIVER=dummy
# And no gamepad, for the same reason there is no display: this run has to say
# the same thing whatever is plugged into the machine it is on. `open_gamepad`
# logs the pad it found and which letter that pad confirms with — a correct and
# useful line, and under the rule below it is also a failure, so every scene
# failed on any developer's desk with a controller attached while CI, which has
# none, stayed green. That is the worst shape a check can have: it fires
# everywhere except where it would be read. The hint is "allow only this device"
# naming a vendor and product that cannot exist, which is how SDL is asked to
# ignore all of them.
export SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT=0x0000/0x0000
# Leaks are a separate question and SDL's own allocations would drown the
# signal; what this run is for is memory errors and undefined behaviour.
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1,halt_on_error=1}"

# A full template rather than `mktemp -t chuck-smoke`, because the two mktemps
# do not agree about what `-t` means. BSD's — the one on macOS — takes it as a
# prefix and appends the random part itself; GNU's takes the argument as a
# template and refuses it outright with "too few X's in template", which is an
# exit before the first scene has run. So this target passed for everyone
# developing on a Mac and had never once completed on Linux, which is the only
# place CI runs it: the job that exists to execute the renderers under a
# sanitizer was failing in its own setup, and the fifteen sectors it claims to
# cover were being covered by nobody. Spelling the whole path with the X's in
# it is the form both accept.
log=$(mktemp "${TMPDIR:-/tmp}/chuck-smoke.XXXXXX") || exit 1
trap 'rm -f "$log"' EXIT INT TERM

failures=0

# run_scene LABEL SECONDS [ARGS...] — SECONDS is how long this screen needs to
# reach its last beat, which for most of them is just the base.
run_scene() {
    label=$1
    dwell=$2
    shift 2
    : > "$log"
    "$BINARY" "$@" > "$log" 2>&1 &
    pid=$!
    # Give it real frames, then ask it to stop the way a window's close box
    # would; SIGTERM reaches SDL_AppQuit, so shutdown is exercised too.
    sleep "$dwell"
    if ! kill -0 "$pid" 2>/dev/null; then
        wait "$pid" 2>/dev/null || true
        printf 'smoke: %-12s EXITED EARLY\n' "$label" >&2
        sed 's/^/    /' "$log" >&2
        failures=$((failures + 1))
        return
    fi
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true

    if [ -s "$log" ]; then
        printf 'smoke: %-12s OUTPUT\n' "$label" >&2
        sed 's/^/    /' "$log" >&2
        failures=$((failures + 1))
    else
        printf 'smoke: %-12s ok\n' "$label"
    fi
}

# The title screen also covers audio_init, the whole synth table and the intro.
run_scene "title" "$SECONDS_PER_SCENE"

sector=1
while [ "$sector" -le "$SECTORS" ]; do
    run_scene "sector $sector" "$SECONDS_PER_SCENE" --level "$sector"
    sector=$((sector + 1))
done

# And every sector again with a hand on the controls, which is the other half
# of what a boot covers and the half this file was missing for as long as it
# has existed.
#
# **Booting a sector executes a sector standing still.** The run above opens
# each floor and presses nothing, so everything behind a player *acting* was
# executed by nothing in the tree: measured with llvm-cov, twelve live drawing
# functions came back at zero — the downed bodies the whole body-discovery rule
# is built on, the crawl, the hacking pose, both bazooka poses, the muzzle
# flash, both rocket sprites and the alarm lighting pass. That is the same hole
# as the credits skyline and the seven unread manual sheets, found a fourth
# time, and `--scene` could not close it: `--scene` opens a screen, and these
# are states inside a sector.
#
# `--demo` is the answer and it keeps `--scene`'s rule — nothing is assembled by
# hand, the game is simply played into those states by a script. Each floor gets
# a longer dwell than the still run above, because the script is a lap: it
# crawls, fires, cycles the weapons, sends a rocket flat and another one
# straight up, and walks at the sector's live terminal, where hacking raises the
# alarm and the alarm walks the floor's guards into range of the next shot.
# DEMO_LAP in src/demo.c is the length of one lap; two of them is the dwell, so
# a hand that misses a beat on the first pass takes it on the second.
define_of demo_lap DEMO_LAP src/demo.c
DEMO_SECONDS=$(seconds "$(awk -v lap="$demo_lap" 'BEGIN { print lap * 2 }')")

sector=1
while [ "$sector" -le "$SECTORS" ]; do
    run_scene "demo $sector" "$DEMO_SECONDS" --level "$sector" --demo
    sector=$((sector + 1))
done

# Everything the fifteen sectors above cannot reach, because a sector is
# entered and never finished, paused, failed or read about. Each of the timed
# ones is given its own length; a cutscene that runs on past its dwell is a
# cutscene only the first seconds of which have ever been executed.
run_scene "abduction" "$ABDUCTION_SECONDS" --scene abduction
run_scene "drive" "$DRIVE_SECONDS" --scene drive
run_scene "arrival" "$ARRIVAL_SECONDS" --scene arrival
run_scene "options" "$SECONDS_PER_SCENE" --scene options
# The sheet's second page, and it needs a name of its own because it is a page
# rather than a state: `--scene options` draws the first one and nothing at all
# draws the nine binding rows, their keycaps or the squeeze that fits a long
# page into the frame. It opens with a capture armed, so the "press a key" art
# is executed too.
run_scene "controls" "$SECONDS_PER_SCENE" --scene controls
run_scene "outro" "$OUTRO_SECONDS" --scene outro
run_scene "credits" "$CREDITS_SECONDS" --scene credits

# The book, a sheet at a time. Nothing turns a page but a hand, so this is the
# only thing in the tree that draws seven of the eight illustrations.
page=1
while [ "$page" -le 8 ]; do
    run_scene "manual $page" "$SECONDS_PER_SCENE" --scene manual --page "$page"
    page=$((page + 1))
done

# `--level 9` gives the screens that report on a run something real to report.
run_scene "report" "$REPORT_SECONDS" --level 9 --scene report
for scene in pause cleared continue gameover; do
    run_scene "$scene" "$SECONDS_PER_SCENE" --level 9 --scene "$scene"
done

# The four restrooms, which are the one playable screen booting a sector never
# opens: a `U` is only walked through by playing. There was one room behind all
# four doors for a long time and it was drawn by nothing in this tree at all;
# there are four now, each a different shape, and a room's whole interior is
# derived from its own wall bounding box — so this is four sets of numbers that
# derivation has never been run on, in a renderer the core suite cannot link.
for sector in 1 5 9 14; do
    run_scene "restroom $sector" "$SECONDS_PER_SCENE" \
        --level "$sector" --scene restroom
done

if [ "$failures" -ne 0 ]; then
    echo "smoke: $failures scene(s) failed" >&2
    exit 1
fi

echo "smoke: title screen, $SECTORS sectors, all eight manual sheets and every"
echo "smoke: named scene ran clean, each for the whole of its own length"
