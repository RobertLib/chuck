#!/bin/bash
#
# Build the Linux payload: the game, the library it needs, and nothing installed.
#
# The development build links whatever SDL3 the machine has through pkg-config,
# which is right for a machine that has one. A player's has not: SDL3 is new
# enough that most distributions in use today ship no package for it at all, so a
# build that says "install libsdl3" is a build that does not run. The library
# therefore travels inside the payload, exactly as it travels inside the macOS
# bundle, and the binary is told to look beside itself for it.
#
# `$ORIGIN/lib` is what does that, and it is the whole trick: the runtime loader
# expands it to the directory the executable is actually in, so the payload can
# be unpacked anywhere — a home directory, a USB stick, wherever the itch app
# puts it — and still find its own copy. Without it the loader searches the system
# paths, finds either nothing or somebody else's build, and the failure lands as
# a line about a shared object rather than as anything to do with this game.
#
# SDL is built from the same pin packaging/fetch_sdl3.sh holds, read out of that
# file rather than written down again here, for the reason that file gives: a
# shipped binary has to be traceable to the library it was linked against. It is
# built with its video and audio backends left to cmake's own detection, which is
# the one real difference from the sanitizer job in CI — that job switches X11 and
# Wayland off because it opens no window, and this one is for somebody who has a
# screen.
set -euo pipefail

# First, because it is the most fundamental thing that can be wrong with running
# this and every message below reads as advice on a machine where the advice
# applies. The game is linked with the host `cc` against a host-built SDL, so
# there is no cross-build here to be had: on a Mac this would otherwise build a
# macOS binary, tar it as `linux-x86_64` and say `upload this`.
if [ "$(uname -s)" != "Linux" ]; then
    echo "build_linux: builds the Linux payload and needs a Linux;" >&2
    echo "            $(uname -s) cannot produce one. See AGENTS.md on which" >&2
    echo "            machine can make which archive." >&2
    exit 1
fi

root=$(cd "$(dirname "$0")/.." && pwd)
dist=${DIST_DIR:-$root/dist}
sdl_prefix=${SDL_PREFIX:-$root/build/sdl3-linux}
jobs=${JOBS:-$( (nproc 2>/dev/null || echo 4) )}

sdl_version=$(sed -n 's/^SDL_VERSION="\(.*\)"$/\1/p' "$root/packaging/fetch_sdl3.sh")
if [ -z "$sdl_version" ]; then
    echo "build_linux: could not read SDL_VERSION from packaging/fetch_sdl3.sh" >&2
    exit 1
fi

define() { sed -n "s/^#define $1 \"\(.*\)\"/\1/p" "$root/src/version.h"; }
version=$(define CHUCK_VERSION)
app_name=$(define CHUCK_APP_NAME)
# See the note in build_windows.sh: the save directory is the header's to name,
# and this README is the only place a player reads it.
app_org=$(define CHUCK_APP_ORG)

# Staged under the name the archive is called, so unpacking it leaves one folder
# with the game's name and version on it rather than a bare `chuck` and a `lib`
# loose in whoever's Downloads directory.
payload=$app_name-$version-linux-x86_64
stage=$dist/stage/$payload

# ---- what a missing X extension costs -------------------------------------
#
# SDL's cmake treats an X extension it cannot find as a hard configure error
# rather than a backend it goes without, and it finds out one extension at a
# time: the break that prompted this printed twenty lines of successful checks
# and then `Couldn't find dependency package for XTEST`, having already spent
# the clone and most of a minute of configuring to get there. The package that
# was missing is four words; finding that out was the whole job.
#
# So the headers are looked for first, and the message names the Debian package
# because that is the distribution the payload is built on
# (`.github/workflows/payloads.yml` pins the runner, and
# itch/install-instructions.md tells a player which one). Other distributions
# spell these differently and the header path is the same on all of them, which
# is why the check is on the header.
#
# **This list is advisory and cmake stays the authority**, which is the only
# honest thing to say about a copy of somebody else's build requirements. If SDL
# adds an extension nobody has added here, this passes and cmake fails exactly as
# it did today — the check can be behind, it cannot be wrong. Nothing in this
# repository can hold it, because the fact it would be held against lives in
# SDL's own tree.
preflight_x11()
{
    local missing=""
    local row header package
    for row in \
        "X11/Xlib.h|libx11-dev" \
        "X11/Xutil.h|libx11-dev" \
        "X11/extensions/Xext.h|libxext-dev" \
        "X11/Xcursor/Xcursor.h|libxcursor-dev" \
        "X11/extensions/XInput2.h|libxi-dev" \
        "X11/extensions/Xfixes.h|libxfixes-dev" \
        "X11/extensions/Xrandr.h|libxrandr-dev" \
        "X11/extensions/Xrender.h|libxrender-dev" \
        "X11/extensions/scrnsaver.h|libxss-dev" \
        "X11/extensions/XTest.h|libxtst-dev" \
        "xkbcommon/xkbcommon.h|libxkbcommon-dev"
    do
        header=${row%%|*}
        package=${row##*|}
        if [ ! -e "/usr/include/$header" ] &&
           [ ! -e "/usr/local/include/$header" ]; then
            case " $missing " in
            *" $package "*) ;;
            *) missing="$missing $package" ;;
            esac
        fi
    done
    if [ -n "$missing" ]; then
        echo "build_linux: SDL will not configure without these:$missing" >&2
        echo "            sudo apt-get install -y --no-install-recommends$missing" >&2
        echo "            (or point SDL_PREFIX at an existing SDL3 install)" >&2
        exit 1
    fi
}

# ---- SDL, once per pin ----------------------------------------------------
# Kept in build/ rather than rebuilt every time: it is four megabytes of
# somebody else's library and it is the same bytes until the pin moves. The
# marker is the library itself, so a half-finished build does not read as a
# finished one.
if [ ! -f "$sdl_prefix/lib/libSDL3.so" ]; then
    for tool in cmake git cc; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            echo "build_linux: needs $tool to build SDL $sdl_version" >&2
            echo "            (or point SDL_PREFIX at an existing SDL3 install)" >&2
            exit 1
        fi
    done
    # Before the clone rather than after it: the point is to fail in the second
    # it takes to stat eleven files, not in the minute it takes to get to the
    # cmake line that would have said the same thing.
    preflight_x11
    src=$root/build/SDL-$sdl_version
    if [ ! -d "$src" ]; then
        echo "build_linux: fetching SDL $sdl_version"
        git clone --depth 1 --no-tags --single-branch \
            --branch "release-$sdl_version" \
            https://github.com/libsdl-org/SDL.git "$src.part"
        mv "$src.part" "$src"
    fi
    echo "build_linux: building SDL $sdl_version into $sdl_prefix"
    cmake -S "$src" -B "$root/build/sdl3-linux-build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$sdl_prefix" \
        -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
    cmake --build "$root/build/sdl3-linux-build" --parallel "$jobs"
    cmake --install "$root/build/sdl3-linux-build"
fi

# ---- the game -------------------------------------------------------------
# `'$$ORIGIN/lib'` gets through **two** expansions, and both of them have to be
# accounted for or the payload only runs on the machine that built it.
#
# make eats one `$`, so `$$ORIGIN` reaches the recipe as `$ORIGIN` — and the
# recipe is a *shell* command, where an unquoted `$ORIGIN` is an unset variable
# that expands to nothing. That leaves `-Wl,-rpath,/lib`: a binary that looks for
# its SDL in the system's own `/lib`, finds either nothing or somebody else's, and
# fails with a line about a shared object rather than anything to do with this
# game. The single quotes are what stop the second expansion; they are consumed by
# that shell and the linker gets the literal string it needs.
#
# This was written the wrong way first and measured rather than reasoned about:
# `make -n` with these overrides prints the link line, and the wrong form prints
# `/lib` in it plainly.
make -C "$root" -j"$jobs" \
    BUILD_DIR=build/linux TARGET=build/linux/chuck \
    CFLAGS="-std=c17 -Wall -Wextra -Wpedantic -O2 -I$sdl_prefix/include" \
    LDFLAGS="-L$sdl_prefix/lib -lSDL3 -lm -Wl,-rpath,'\$\$ORIGIN/lib'" \
    all

# ---- the payload ----------------------------------------------------------
rm -rf "$stage"
mkdir -p "$stage/lib"
install -m 755 "$root/build/linux/chuck" "$stage/chuck"
# Stripped for the same reason the .app's binary is: debug symbols are most of
# the download and none of the game. `|| true` because a toolchain without
# `strip` is a smaller payload's worth of nothing to stop a release for.
strip "$stage/chuck" 2>/dev/null || true

# The versioned file and the SONAME symlink beside it, because the binary asks
# for `libSDL3.so.0` and a payload holding only `libSDL3.so.0.4.14` answers a
# question nobody asked.
for so in "$sdl_prefix"/lib/libSDL3.so*; do
    cp -a "$so" "$stage/lib/"
done

"$root/packaging/itch_manifest.sh" "$stage" chuck

# A README in the payload, because a browser download is a directory somebody
# unpacked and the itch app's launch button is not there to help them. Three
# lines: what to run, what the folder beside it is, and where the saves go.
cat >"$stage/README.txt" <<TXT
$app_name $version — Linux x86_64

Run ./chuck

lib/ holds the SDL3 this build was made against; the game looks for it there
and needs nothing installed. Keep the two together.

Settings and progress are kept in \$XDG_DATA_HOME/$app_org/$app_name (usually
~/.local/share/$app_org/$app_name) and nowhere else. Deleting that directory starts
the tower again.
TXT

archive=$dist/$payload.tar.gz
rm -f "$archive"
tar -C "$dist/stage" -czf "$archive" "$payload"
# The staging tree is an intermediate, not an artifact. Left standing it makes
# dist/ hold the payload twice — the archive and a loose copy of everything in it
# — which is how somebody comes to upload the folder instead of the zip.
rm -rf "$dist/stage"

echo "build_linux: $archive ($(du -h "$archive" | cut -f1)) — upload this"
