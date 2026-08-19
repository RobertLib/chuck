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
    local name=$1 header=$2 value alias
    value=$(sed -n "s/^#define $name  *\([0-9][0-9.]*\)f\{0,1\}.*/\1/p" \
        "$root/src/$header" | head -1)
    if [ -z "$value" ]; then
        # One level of indirection, because a constant in this tree is
        # deliberately sometimes *another* constant rather than a literal:
        # `SECTOR_TALLY_HOLD_TIME` is `CHATTER_HOLD_TIME` so that "how long does
        # a sentence stay readable" has one answer instead of two numbers with
        # nothing holding them together. A reader that can only follow literals
        # cannot follow that, and the alternative was this file keeping a second
        # copy of the relationship — which is the defect the whole of
        # `tools/check_lists.py` exists for.
        alias=$(sed -n "s/^#define $name  *\([A-Z_][A-Z0-9_]*\).*/\1/p" \
            "$root/src/$header" | head -1)
        if [ -n "$alias" ]; then
            define_of "$alias" "$header"
            return
        fi
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
        # The one card that does *not* hold still until a hand moves it: it runs
        # down and then loads whatever comes next. Filed with the sheets below
        # for as long as it was unnamed, so two thirds of its budget drew the
        # card and the last third simulated the sector after it — which is the
        # very thing the comment under this case list says cannot happen.
        cleared) span=$(define_of LEVEL_CLEARED_DISPLAY_TIME game_config.h) ;;
        # And the reveal became the same kind of thing the moment it stopped
        # being as long as the map. `level_reveal_hold_for` holds it open for the
        # line drawn over it, so this is a beat with a clock of its own rather
        # than a card that waits for a hand — and left in the default below, a
        # two-second budget would be spent entirely inside the animation and
        # never reach the frame the sector opens on, which the reveal *did* reach
        # back when it was over in 0.18s. Truncation arriving as a side effect of
        # a fix somewhere else is this script's own recurring defect.
        reveal) span=$(define_of SECTOR_TALLY_HOLD_TIME game_config.h) ;;
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

# And the one thing in this binary that *produces* something rather than checking
# something, which no gate had ever run.
#
# `make coverage-shell` prints the functions neither the suite nor this sweep
# executes. The whole of `screenshot.c` was on that list — `screenshot_write`,
# `shot_plan_open`, `shot_plan_broke` and `shot_frame_path` — along with
# `main.c`'s `parse_shot_number`. That is every line of `--shot`, which is the
# only way a picture of this game exists at all: there is no art in this
# repository to crop, so the store page, the README and every bug report get
# their frames out of a running process. A break in it would have been found by
# whoever was next cutting press assets, which is the same sentence AGENTS.md
# already spends a paragraph on about `make app`.
#
# Two runs, because a capture has two shapes and they take different paths. One
# still is `shot_plan_open` with a frame count of one and `screenshot_write`
# straight out of `game_render`. A burst is the synthetic clock: the world is
# stepped at `1/F` per drawn frame, `shot_frame_path` numbers the files, and the
# plan closes the process when the last one lands.
#
# **And the files have to be there afterwards**, which is the half a status code
# cannot answer. `shot_plan_broke` already turns an unwritable frame into a
# failing exit — but a capture that silently wrote nowhere, or wrote one file
# where six were asked for, would exit 0 and print a line saying so. That is this
# repository's own recurring defect aimed at the switch written to end it, so the
# count is read off the disk rather than off the log.
#
# **And so does the size of them**, which is the half counting files cannot see.
# A capture is the render target read back, and under letterbox presentation that
# is the *window* — so a machine whose saved settings said `fullscreen 1` wrote
# every frame at its own display size with the logical frame scaled into it by
# whatever non-integer factor that display implied. Measured before the fix: this
# developer's machine produced 1024x706 rather than 800x552, pre-mangling the
# pixel art before `tools/press_kit.sh` had even run its `-resize 200%`, while
# the MANIFEST that script writes claimed 800x552 outright. A capture of the
# wrong *size* writes a file and sails through a file count, exactly the way a
# capture of the wrong *moment* does — so the frame is measured, not counted.
#
# `--shot` now makes the run a scripted one (see `PlatformState.scripted`), which
# leaves the window at the logical frame whatever the runner's file says. This is
# what holds that: the two figures come off `src/game_config.h` rather than being
# written down here, because a resolution spelled in a shell script is a second
# copy of a `#define` and this repository has a file full of notes about those.
view_w=$(sed -n 's/^#define VIEW_W \([0-9]*\).*/\1/p' "$root/src/game_config.h")
view_h=$(sed -n 's/^#define VIEW_H \([0-9]*\).*/\1/p' "$root/src/game_config.h")
if [ -z "$view_w" ] || [ -z "$view_h" ]; then
    echo "soak: could not read VIEW_W/VIEW_H from src/game_config.h" >&2
    exit 1
fi

# The width and height a BMP says it is, out of its own header: bytes 18 and 22,
# little-endian, height signed because a bottom-up bitmap writes it positive and
# a top-down one negative. `od` rather than a language, because this script has
# no interpreter of its own and the whole question is eight bytes.
bmp_size()
{
    local file=$1 w h
    w=$(od -An -tu4 -j18 -N4 "$file" | tr -d ' ')
    h=$(od -An -td4 -j22 -N4 "$file" | tr -d ' ')
    [ -z "$w" ] && return 1
    [ -z "$h" ] && return 1
    printf '%sx%s' "$w" "${h#-}"
}

soak_shot()
{
    local what=$1
    local expected=$2
    shift 2

    local dir status=0 found
    dir=$(mktemp -d "${TMPDIR:-/tmp}/chuck-shot.XXXXXX") || dir=""
    if [ -z "$dir" ]; then
        echo "soak: could not create a temporary directory for $what"
        failures=$((failures + 1))
        return
    fi

    "$binary" --shot "$dir/frame.bmp" "$@" >"$log" 2>&1 || status=$?
    if [ "$status" -ne 0 ]; then
        echo "soak: $what exited $status"
        sed 's/^/      /' "$log"
        failures=$((failures + 1))
        rm -rf "$dir"
        return
    fi
    if grep -qE "$complaints" "$log"; then
        echo "soak: $what had something to say"
        grep -E "$complaints" "$log" | sed 's/^/      /'
        failures=$((failures + 1))
        rm -rf "$dir"
        return
    fi
    # A single frame keeps the name it was given; a burst is numbered. Counting
    # whatever landed covers both without this script having to know which.
    found=$(find "$dir" -type f -name 'frame*.bmp' | wc -l | tr -d ' ')
    if [ "$found" -ne "$expected" ]; then
        echo "soak: $what wrote $found frame(s), not $expected"
        sed 's/^/      /' "$log"
        failures=$((failures + 1))
        rm -rf "$dir"
        return
    fi
    # And a BMP header on the first of them, because an empty file is a file.
    local first
    first=$(find "$dir" -type f -name 'frame*.bmp' | head -1)
    if [ ! -s "$first" ]; then
        echo "soak: $what wrote an empty frame"
        failures=$((failures + 1))
        rm -rf "$dir"
        return
    fi
    # And it is the logical frame rather than whatever window this machine
    # happens to open. See the note above `bmp_size`: this is the assertion that
    # a capture is the same picture everywhere, which is what makes it usable as
    # a measurement at all.
    local got
    got=$(bmp_size "$first") || got="unreadable"
    if [ "$got" != "${view_w}x${view_h}" ]; then
        echo "soak: $what wrote a ${got} frame, not ${view_w}x${view_h}"
        failures=$((failures + 1))
        rm -rf "$dir"
        return
    fi
    rm -rf "$dir"
    echo "soak: $what ok, at $got"
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

# `reveal` is the sector coming up with the tally riding over it, which is where
# ten of `TRANSITION_INTEL`'s sixteen lines and eleven of the seventeen clears'
# numbers land — and it was a frame no gate drew, because `cleared` was the only
# staged screen that put a tally on the glass and it is the other placement of
# the same band. Not filed as timed: `level_reveal_init` runs its batches out in
# under half a second, so the frame this exists for is the start of the budget
# and the rest of it plays the floor, which is what `--level N` does anyway.
screens=(abduction chase opening manual settings pause report cleared reveal
         continue gameover outro credits resume restroom aftermath)
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
    # An enum rather than a #define, which is what it is today: counted here
    # rather than written down, so a page is walked by having been added. It has
    # happened twice since this line was written — RECORDS, then DIFFICULTY — and
    # the sweep needed no edit either time, which is the whole point. The
    # sentence naming the two pages that existed then did need one, and did not
    # get it until somebody read this file looking for something else.
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

# The poses of a sector after it went wrong. `draw_player` answers hacking first
# and crawling second, so one frame holds one of them; between them these pages
# reach the bodies, the opened patch, the alarm lighting, the emitting half of
# the particle system, both the flat and the vertical bazooka, the muzzle flash
# and the throwable drawn in the hand on a rung.
# See `soak_stage_aftermath` in src/game.c for why the state is staged rather
# than played, and which renderer each pose is the only route to.
#
# The count comes off `AFTERMATH_POSE_COUNT` for the same reason the options
# sheet's does off `SETTINGS_PAGE_COUNT`: it was a literal here and a `switch`
# there, the two of them agreed on five, and the sentence above them said four.
# A pose is walked now by having been added.
poses_available=$(sed -n 's/^#define AFTERMATH_POSE_COUNT \([0-9]*\).*/\1/p' \
    "$root/src/game.h")
if [ -z "$poses_available" ] || [ "$poses_available" -lt 1 ]; then
    echo "soak: could not count the aftermath's poses in src/game.h" >&2
    exit 1
fi
aftermath_poses=($(seq 1 "$poses_available"))
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
# Every pose rather than the crawl alone, because which page is the crawl is
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

# Every report, because `--screen report` above only reaches sector 1's.
#
# The report is the placement that gives a row of `TRANSITION_INTEL` a whole
# screen, and six sectors reach one — and this switch drew one of them: the
# staging spelled the clear of sector one whatever `--level` said, so five of
# the six lines the plot rests on had never been rasterized by anything. Its
# twin `--screen reveal` was given exactly this treatment a release earlier, and
# the comment on that one closes "a placement is not covered because its twin
# is". This is the twin.
#
# Which sectors show one is a property of the maps rather than a list: every one
# that is not the last and does not leave by a `Y`. Grepped for the reason the
# restrooms below are, so a map that gains or loses a window is walked by having
# been edited. A facade carries a `Y` too, which is right — a climb is left by
# its window and reports nothing.
#
# `$seconds` rather than the beat's own length, unlike the run in the list
# above: what these add is the line and the door plate, both of which are on the
# glass inside a second, and the rest of the sequence is the same drawing on
# every sector. The one full-length walk is the one the screens loop already
# did.
#
# Skipped whole in `smoke` for the reason the screens loop skips `report`
# itself: the mode's own claim is that it held no timed sequence, and six short
# walks of a screen it has just said it skipped would be a summary arguing with
# itself.
reports=0
if [ "$mode" != smoke ]; then
    for sector in $(seq 1 $((sectors - 1))); do
        if grep -E '^[#. ]' "$root/levels/level$sector.txt" | grep -q 'Y'; then
            continue
        fi
        soak_one "report after sector $sector" "$seconds" \
            --screen report --level "$sector"
        reports=$((reports + 1))
    done
    if [ "$reports" -lt 1 ]; then
        echo "soak: found no sector that leaves by a stair door to report" \
             "after" >&2
        exit 1
    fi
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

# The captures. `--shot-fps` is held to `MIN_FRAME_RATE` or better by
# `shot_plan_open`, so the burst asks for a rate the clamp cannot shorten; three
# frames is enough to reach the numbering and the synthetic clock without adding
# a second to the sweep.
#
# On sector 1 rather than a screen, because a capture of a card is a capture of a
# still world and the synthetic clock is the half worth exercising.
soak_shot "one still" 1 --level 1 --shot-at 1.0
soak_shot "a three-frame burst" 3 --level 1 --shot-at 1.0 \
    --shot-frames 3 --shot-fps 24
shots=2

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

# ---------------------------------------------------------------------------
# And what the switch says when it turns a request down.
#
# `--screen` refuses for nine different reasons and only one of them is the name.
# For a release the caller printed the list of screen names for all nine, so
# `--screen manual --page 99` answered "Sheet 99 is outside the manual's 10" and
# then told the caller their screen name was wrong — with `manual` in the list it
# had just printed. Nothing could see it: the suite links no SDL, so `main.c` is
# on the far side of the boundary, and every other check here drives names that
# work.
#
# Two things per case, because either alone passes the bug: it has to *fail*, and
# it must not name the list. A check that only asked for the failure passed
# throughout, since the old code failed too.
#
# **And a refusal has to arrive**, which is a third thing and needs a watchdog
# rather than an exit status. `--soak inf` logged `Soaking for inf seconds, then
# closing` and then never closed: `SDL_atof` reads `inf` happily, it is greater
# than nought, so the range check passed and the budget counted down forever.
# There is no status to check in that case and no output to grep — the run simply
# does not end, which is the one failure a script cannot report on its own
# behalf. So every case here is given a clock, and a case that outlives it is
# named as what it is rather than left to the workflow's `timeout-minutes`.
refusals=0
refusal_seconds=${SOAK_REFUSAL_SECONDS:-20}

# Runs the binary with a clock on it, and sets `status` to 124 if it outlived
# the clock — `timeout(1)`'s own answer for it, and not a status the game can
# return, so the two cannot be confused.
run_with_a_clock()
{
    "$@" >"$log" 2>&1 &
    local pid=$!
    local waited=0
    while kill -0 "$pid" 2>/dev/null; do
        if [ "$waited" -ge "$refusal_seconds" ]; then
            kill -9 "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
            status=124
            return
        fi
        sleep 1
        waited=$((waited + 1))
    done
    status=0
    wait "$pid" || status=$?
}

refuse()
{
    local what=$1
    local expect=$2
    shift 2

    # `--soak 1` is what makes an *accepted* request here exit at all, so it is
    # appended to every case that does not already carry one. The cases about
    # `--soak` itself do carry one, and a second would be read straight past:
    # the parser returns on the first match it finds.
    case " $* " in
        *" --soak "*) set -- "$@" ;;
        *) set -- "$@" --soak 1 ;;
    esac

    local status=0
    run_with_a_clock "$binary" "$@"
    refusals=$((refusals + 1))
    if [ "$status" -eq 124 ]; then
        echo "soak: $what neither finished nor failed inside" \
             "${refusal_seconds}s; a step that reports the clock instead of the" \
             "cause is the whole reason this switch exists"
        sed 's/^/      /' "$log"
        failures=$((failures + 1))
        return
    fi
    if [ "$status" -eq 0 ]; then
        echo "soak: $what was accepted; a switch a script drives owes it an" \
             "exit code"
        sed 's/^/      /' "$log"
        failures=$((failures + 1))
        return
    fi
    if ! grep -q "$expect" "$log"; then
        echo "soak: $what did not say why; expected to see '$expect'"
        sed 's/^/      /' "$log"
        failures=$((failures + 1))
        return
    fi
    if [ "$expect" != "expects one of" ] && grep -q "expects one of" "$log"; then
        echo "soak: $what named the list of screen names, which is a message" \
             "about a fault this is not"
        sed 's/^/      /' "$log"
        failures=$((failures + 1))
        return
    fi
    echo "soak: $what refused, and said which"
}

refuse "an unknown screen" "expects one of" --screen definitely-not-a-screen
refuse "a sheet past the manual" "outside the manual" --screen manual --page 99
refuse "a pose past the aftermath" "outside the aftermath" \
    --screen aftermath --page 99
refuse "a page past the options sheet" "outside the options sheet" \
    --screen settings --page 99

# And the two sectors that show no report, which is the other half of `--screen
# report` learning to read `--level`. A frame the game cannot produce is refused
# rather than staged, the way the continue card's is: the last sector has
# nothing after it, and a sector that leaves by its window hands the next one
# over without cutting to a screen at all.
refuse "a report after the last sector" "is the last of the campaign" \
    --screen report --level "$sectors"
windowed=""
for sector in $(seq 1 $((sectors - 1))); do
    if grep -E '^[#. ]' "$root/levels/level$sector.txt" | grep -q 'Y'; then
        windowed=$sector
        break
    fi
done
if [ -n "$windowed" ]; then
    refuse "a report after a sector that leaves by its window" \
        "leaves by its window" --screen report --level "$windowed"
else
    echo "soak: found no sector leaving by a window to refuse a report after" >&2
    failures=$((failures + 1))
fi

# The values, rather than the names. Every one of these was accepted before
# `parse_switch_number` read them all through one strict parse: `inf` and
# `1e400` became budgets that never run out, and the two `3s` were quietly read
# as 3 — a sheet number the sweep then logged as `manual sheet ok`, which is
# this file's own recurring defect reached by a typo instead of by a missing
# switch. An infinity is worth both spellings, because one is a word the parser
# has to know and the other is arithmetic it has to survive.
refuse "a soak of inf" "expects a real number" --soak inf
refuse "a soak that overflows to inf" "expects a real number" --soak 1e400
refuse "a soak with junk on the end" "expects a number" --soak 3s
refuse "a sheet number with junk on the end" "expects a number" \
    --screen manual --page 3s
refuse "a sheet number that is not whole" "whole sheet number" \
    --screen manual --page 2.5

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
     "$reports reports, $restrooms restrooms and $editors editor run(s) drew" \
     "clean — $held —" \
     "$shots capture(s) landed on disk and $refusals bad request(s) were" \
     "turned down naming the right thing"
if [ -n "$skipped" ]; then
    echo "soak: this was SOAK_MODE=smoke; it skipped$skipped." \
         "SOAK_MODE=full is the sweep those are covered by."
fi
