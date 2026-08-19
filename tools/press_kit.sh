#!/bin/bash
#
# Photograph the game: twenty stills, five animations, the itch.io cover and a
# wallpaper, into dist/press/ with a MANIFEST.txt naming what each file is.
#
# It exists because there is no art in this repository to crop. Every pixel is
# drawn at runtime, so the only place a picture of this game exists is the back
# buffer of a running process — which is what `--shot` reads (see
# src/screenshot.h). That also means the pictures can be *rebuilt* after a change
# to the art rather than re-photographed by hand.
#
# Needs ImageMagick for everything past the capture itself: the game writes BMPs,
# and a store page wants PNGs, a cover at a fixed size and a GIF or two. On a Mac
# `sips` covers the conversion alone, and the script says what it could not
# produce without ImageMagick rather than quietly leaving it out.
#
# It reads and writes nothing of whoever runs it, and this paragraph used to say
# the opposite: "it reads and writes the settings and progress of whoever runs it,
# the same as playing the game does. So the title screen carries a resume chip if
# this machine has one, and the record sheet shows this machine's times."
#
# That was true, and it was the bug. Seventy lines below, the MANIFEST this
# script writes into every press kit said the captures were taken "at the window
# the game opens (800x552) and the settings it ships with" — so one file made both
# claims and only one of them could hold. The MANIFEST was the one that was wrong,
# and not by a little: `game_init` applied the saved `fullscreen`, and a capture
# is the render target read back, which under letterbox presentation is the
# *window*. On a machine with the flag on, every frame here came out at that
# display's size with the logical frame scaled into it by a non-integer factor —
# measured, 1024x706 against 800x552, so the pixel art arrived already mangled
# before the `-resize 200%` below had run, on the one pipeline that goes to
# trouble over exactly that (see the cover note further down). The runner's CRT
# filter, reduced motion and assists rode in the same way: MORE HEARTS on meant
# five hearts in the HUD of every still.
#
# `--shot` makes the run a scripted one now (`PlatformState.scripted` in
# src/game.h): shipped defaults, no saved fullscreen, neither file read nor
# written. Which is what `--seed` was already half of — a capture is a
# measurement and a measurement has to be repeatable, and a seed only pins the
# part of the frame that comes out of the RNG. `tools/soak.sh` measures the
# frame's size against VIEW_W/VIEW_H so this cannot come back.
set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
binary=${PRESS_BINARY:-$root/chuck}
out=${1:-$root/dist/press}
seed=${PRESS_SEED:-20220314}

# The rate a burst is captured and played back at. One variable rather than two
# numbers, because the delay a GIF is assembled with and the step the world was
# advanced by have to be the same figure or the animation plays at a speed the
# game does not run at. MIN_FRAME_RATE is the floor the game itself refuses
# below; 20 is that floor exactly, which is also a perfectly ordinary GIF rate.
fps=${PRESS_GIF_FPS:-20}
# How long each burst runs, in frames. Three seconds at the rate above.
burst=${PRESS_GIF_FRAMES:-60}

case $binary in
    /*) ;;
    *) binary=$(cd "$(dirname "$binary")" && pwd)/$(basename "$binary") ;;
esac
if [ ! -x "$binary" ]; then
    echo "press: no executable at $binary (run: make)" >&2
    exit 1
fi

# ImageMagick 7 calls itself `magick`, 6 calls itself `convert`, and macOS has
# `sips` whatever happens. The first two can do all of this; the third can only
# convert, so what it cannot produce is named rather than silently missing.
im=""
if command -v magick >/dev/null 2>&1; then
    im=magick
elif command -v convert >/dev/null 2>&1; then
    im=convert
fi
if [ -z "$im" ] && ! command -v sips >/dev/null 2>&1; then
    echo "press: needs ImageMagick (magick/convert) or macOS sips" >&2
    exit 1
fi

work=$(mktemp -d "${TMPDIR:-/tmp}/chuck-press.XXXXXX") || work=""
if [ -z "$work" ]; then
    echo "press: could not create a temporary directory in ${TMPDIR:-/tmp}" >&2
    exit 1
fi
log=$work/run.log

# Headless, on the software renderer, so this needs no window and no audio
# device: a busy or absent one must not be the reason a picture is missing.
export SDL_VIDEODRIVER=dummy
export SDL_AUDIODRIVER=dummy

complaints='runtime error|AddressSanitizer|SUMMARY: .*Sanitizer|ERROR: |Could not |is outside the campaign'

failures=0
taken=0

mkdir -p "$out"
manifest=$out/MANIFEST.txt
# The frame's own size, off the header that defines it rather than written down
# here. It was the literal "800x552" for as long as this line existed, next to a
# header paragraph explaining that the captures came out at whatever window the
# runner's settings opened — so the one number in this file a reader would trust
# was the one nothing held. VIEW_W/VIEW_H is the single answer now, and
# `tools/soak.sh` asserts the captures actually have it.
view_w=$(sed -n 's/^#define VIEW_W \([0-9]*\).*/\1/p' "$root/src/game_config.h")
view_h=$(sed -n 's/^#define VIEW_H \([0-9]*\).*/\1/p' "$root/src/game_config.h")
if [ -z "$view_w" ] || [ -z "$view_h" ]; then
    echo "press: could not read VIEW_W/VIEW_H from src/game_config.h" >&2
    exit 1
fi
{
    echo "# What each file here is, and the command that produced it."
    echo "#"
    echo "# Rebuilt by tools/press_kit.sh against $(cd "$root" && git rev-parse --short HEAD 2>/dev/null || echo 'an untracked tree')."
    echo "# Everything is captured headless on the dummy video driver, at the"
    echo "# game's logical frame (${view_w}x${view_h}) and the settings it ships"
    echo "# with: --shot makes the run a scripted one, so nothing on the disk of"
    echo "# whoever runs this reaches the pictures and nothing here reaches theirs."
    echo
} >"$manifest"

# One capture: run the game with the switches given, then say so or say why not.
#
# The checks are the sweep's, because the failure modes are: a process that dies
# before it writes, a build that ignored the switch and drew one frame, a
# sanitizer with something to say. A press run that quietly produced nothing is
# how a store page ends up showing the previous version.
capture()
{
    local name=$1
    shift

    # Pinned, so the same commit photographs the same night.
    #
    # The header above says the pictures can be *rebuilt* after a change, and
    # for a release that was not true of anything in them: the simulation was
    # seeded from `time(NULL)` and SDL's own stream from the tick count, so
    # every run drew a different live card, guards in different places and
    # different decoration variants, and the presentation animated on the wall
    # clock rather than on the capture's rate. Two runs of this script produced
    # two different press kits from one commit, which makes a still impossible
    # to diff and a regression in one impossible to see.
    #
    # `PRESS_SEED` is overridable because the value is arbitrary and somebody
    # re-cutting the store page may simply prefer a different arrangement — what
    # matters is that it is the same one twice.
    local status=0
    "$binary" --seed "$seed" --shot "$work/$name.bmp" "$@" >"$log" 2>&1 ||
        status=$?
    if [ "$status" -ne 0 ]; then
        echo "press: $name exited $status"
        sed 's/^/       /' "$log"
        failures=$((failures + 1))
        return 1
    fi
    if grep -qE "$complaints" "$log"; then
        echo "press: $name had something to say"
        grep -E "$complaints" "$log" | sed 's/^/       /'
        failures=$((failures + 1))
        return 1
    fi
    if ! grep -q "Wrote .* frame" "$log"; then
        echo "press: $name wrote nothing; --shot was not honoured"
        sed 's/^/       /' "$log"
        failures=$((failures + 1))
        return 1
    fi
    taken=$((taken + 1))
    return 0
}

# A still, at native size and again at twice it.
#
# Both, because they answer different questions. itch.io wants a screenshot no
# wider than 1920 and displays it in a gallery a third that size, so the doubled
# one is what looks sharp there; the native one is what a reader wants for a bug
# report, and it is the only one that is the game pixel for pixel. The double is
# a point upscale — nearest neighbour — because this art is drawn on a pixel grid
# and any interpolating filter is a photograph of it through frosted glass.
still()
{
    local name=$1 lead=$2 caption=$3
    shift 3

    capture "$name" --shot-at "$lead" "$@" || return
    if [ -n "$im" ]; then
        "$im" "$work/$name.bmp" -strip "$out/$name.png"
        "$im" "$work/$name.bmp" -filter point -resize 200% -strip \
            "$out/$name@2x.png"
    else
        sips -s format png "$work/$name.bmp" --out "$out/$name.png" >/dev/null
    fi
    # The seed is in the line because the line is a promise that this is the
    # command behind the file, and without it the command reproduces a different
    # picture.
    printf '%-28s %s\n%-28s   $ chuck --seed %s --shot %s.bmp --shot-at %s %s\n\n' \
        "$name.png" "$caption" "" "$seed" "$name" "$lead" "$*" >>"$manifest"
    echo "press: $name.png"
}

# A burst, assembled into a GIF.
#
# The frames are consecutive rather than sampled, and the world was advanced by
# exactly one 1/fps step per frame, so the delay below is the same figure the
# capture used and the animation plays at the speed the game runs at. `-layers
# optimize` is what keeps a three-second loop inside the few megabytes a store
# page will autoplay.
animation()
{
    local name=$1 lead=$2 caption=$3
    shift 3

    if [ -z "$im" ]; then
        echo "press: skipping $name.gif; that needs ImageMagick"
        return
    fi
    capture "$name" --shot-at "$lead" --shot-frames "$burst" \
        --shot-fps "$fps" "$@" || return
    "$im" -delay "1x$fps" -loop 0 "$work/$name"-*.bmp \
        -layers optimize -strip "$out/$name.gif"
    printf '%-28s %s\n%-28s   $ chuck --seed %s --shot %s.bmp --shot-at %s --shot-frames %s --shot-fps %s %s\n\n' \
        "$name.gif" "$caption" "" "$seed" "$name" "$lead" "$burst" "$fps" \
        "$*" >>"$manifest"
    echo "press: $name.gif ($(du -h "$out/$name.gif" | cut -f1))"
}

# ---------------------------------------------------------------------------
# The stills.
#
# **The number is the order.** Which picture goes first is a decision somebody has
# to make and a directory listing will not make it, so it is written into the
# filenames. The lead-in is not decoration —
# every screen in this game arrives on a fade or a slide, and the first frame of
# any of them is a picture of nothing much.
#
# Which sector each shot is of is written here rather than derived, and that is
# the one place this script knowingly names a number: a photograph is a choice
# about composition, so "the lobby" and "the first climb" are the point of it.
# `tools/check_docs.py` holds the store page's own prose to the maps; this list
# is held by looking at it.
# ---------------------------------------------------------------------------
still 01-lobby            3.0 "Sector 1: the lobby, and the way in" --level 1
still 02-alarm            2.0 "The alarm up: red light, bodies where they fell, and a way out" --screen aftermath --page 2
still 03-climb            6.0 "The facade: no gravity, no ladders, and a wind that announces itself" --level 15
still 04-offices          5.0 "Four storeys of it, with the night shift on them" --level 2
still 05-drive           12.0 "The prologue drive: stay on the SUV, keep the car whole" --screen chase
still 06-archive          5.0 "The archive floors" --level 9
still 07-panelled         5.0 "Under the executive floor, where the ceiling fans are" --level 14
still 08-plant            5.0 "The back half of the building, in plate carriers" --level 16
still 09-glass            5.0 "Sector 8: the two-key door, and the turn the night takes" --level 8
still 10-manual-crew      2.0 "The field manual: who is in the building, and why" --screen manual --page 2
still 11-manual-quiet     2.0 "The field manual: the quiet way through a floor" --screen manual --page 7
still 12-report           2.5 "The report between sectors" --screen report
still 13-duct             2.0 "Inside the ventilation trunking" --screen aftermath --level 12 --page 2
still 14-restroom         2.0 "A restroom sublevel, off an archive floor" --screen restroom --level 9
# The caption used to read "assists, veteran, records", which is three nouns
# describing a sheet that had since become four pages: the assists and VETERAN
# moved to DIFFICULTY and the figures to RECORDS, and what `--screen settings`
# draws is the front page — audio, display, and three rows that open the other
# three. So the MANIFEST described a frame the shot does not contain, on the
# one file in a press kit whose whole job is saying what the reader is looking
# at. Which page the store shows is the author's call and this list is where it
# is made, and it is still the front page: adding DIFFICULTY as a shot of its
# own would be choosing what the shop shows, which is not a thing an audit
# gets to decide. `--screen settings --page N` is how, if it is ever wanted.
still 15-options          2.0 "The options sheet: audio, display, and the three sheets behind it" --screen settings
still 16-canteen          9.0 "The canteen floor" --level 6
still 17-kerb             4.0 "00:12, three blocks from work" --screen abduction
still 18-title            2.5 "The title screen"
# SPOILER. It is the last frame of the game and it is in here because a press kit
# is not only a store page — a review, a devlog and a trailer's last second all
# want it. `itch/README.md` says which files the store page uses, and this is not
# one of them.
still 19-roof            20.0 "SPOILER: the roof, at 01:00" --screen outro

# ---------------------------------------------------------------------------
# The animations.
#
# Four things in this game move without a hand on the controls, which is what a
# headless capture can photograph moving: a cutscene, the traffic on the drive, a
# floor's own patrols and hazards, and the ending. A GIF of the *player* doing
# something would need input this process has no way to receive — see
# `game_soak_screen` on why an aftermath is staged rather than played, and note
# that a staged frame is frozen on purpose and therefore animates nothing.
# ---------------------------------------------------------------------------
animation 30-loop-lobby   2.0 "A floor with nobody touching it: patrols, lifts, lights" --level 1
animation 31-loop-drive   5.0 "The drive, mid-block" --screen chase
animation 32-loop-kerb    2.0 "The kerb, and the SUV that pulls up between them" --screen abduction
animation 33-loop-climb   3.0 "The wall, in the rain" --level 15
animation 34-loop-roof   18.0 "SPOILER: the roof" --screen outro

# ---------------------------------------------------------------------------
# The derived artwork.
#
# itch.io asks for one image at a fixed shape — 630x500, shown as small as
# 315x250 — and it is the only picture most people will ever see of this game.
# It is cut out of the title screen rather than drawn separately, because the
# title screen is already key art: the logotype, the tagline, the tower with two
# lit floors, and the man on the pavement looking up at it. What the crop drops
# is the prompt row and the chips under it, which are the one part of that frame
# that means nothing to somebody who is not already playing.
#
# Doubled with a point filter and then brought down with a smooth one, rather
# than resized once: a single non-integer resize of pixel art either blurs it or
# leaves rows of uneven width, and going up by a whole number first is what
# avoids both.
# ---------------------------------------------------------------------------
if [ -n "$im" ] && [ -f "$work/18-title.bmp" ]; then
    "$im" "$work/18-title.bmp" -crop 592x470+104+0 +repage \
        -filter point -resize 400% -filter Lanczos -resize 1260x1000\! \
        -strip "$out/cover-630x500@2x.png"
    "$im" "$out/cover-630x500@2x.png" -resize 630x500\! -strip \
        "$out/cover-630x500.png"
    printf '%-28s %s\n\n' "cover-630x500.png" \
        "itch.io cover art, cut from the title screen (and @2x)" >>"$manifest"
    echo "press: cover-630x500.png"

    # A page background, and the one asset here that is a wallpaper rather than a
    # document: 16:9 out of a 1.45:1 frame, so it is the title screen's middle
    # band with the sky above the tower kept and the pavement trimmed.
    "$im" "$work/18-title.bmp" -crop 800x450+0+40 +repage \
        -filter point -resize 400% -filter Lanczos -resize 1920x1080\! \
        -strip "$out/wallpaper-1920x1080.png"
    printf '%-28s %s\n\n' "wallpaper-1920x1080.png" \
        "16:9 wallpaper / page background, from the title screen" >>"$manifest"
    echo "press: wallpaper-1920x1080.png"
fi

if [ "$failures" -ne 0 ]; then
    echo "press: $failures capture(s) failed"
    rm -rf "$work"
    exit 1
fi

rm -rf "$work"
echo "press: $taken captures into $out — see $manifest"
if [ -z "$im" ]; then
    echo "press: no ImageMagick, so no cover, no wallpaper and no GIFs;" \
         "install it and run this again for those"
fi
