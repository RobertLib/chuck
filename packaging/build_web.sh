#!/bin/bash
#
# Build the browser payload: dist/Chuck-<v>-web.zip, an itch.io HTML5 upload.
#
# This is the fourth platform and the first one that is not a download. itch.io
# serves an HTML5 build from an iframe on the game's own page, which is how most
# of that site is actually played — a visitor who has to download a binary,
# unblock it and find it in a Downloads folder is a visitor who has already
# left.
#
# It is the same four steps as build_linux.sh, build_windows.sh and
# build_macos.sh — the library, the game, the payload, the archive — and the
# same rule: this script goes from the tree to one archive in dist/ and stops.
# Nothing here uploads anything.
#
# What is different, and it is the only thing about this file worth reading
# twice:
#
#   **The archive has no folder in it.** Linux and Windows deliberately unpack
#   into one named directory, because the alternative is an exe and a DLL loose
#   in somebody's Downloads. itch.io's HTML5 hosting is the exact opposite: it
#   looks for `index.html` at the *root* of the zip and offers no way to say
#   otherwise, so a payload nested the way its three siblings are nested is a
#   page that will not start and says nothing about why.
#
# SDL is built from source here rather than fetched. The macOS framework, the
# mingw DLL and the Linux .so are all binaries libsdl.org publishes; there is no
# published wasm build, and there could not usefully be one, because the thing
# that makes a player's saves survive closing the tab is a *compile-time* option
# of SDL's own — see the note over the cmake line below. The version is the same
# pin the other three read, out of packaging/fetch_sdl3.sh rather than written
# down again.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
dist=${DIST_DIR:-$root/dist}
jobs=${JOBS:-$( (sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4) )}

# ---- the toolchain --------------------------------------------------------
#
# emcc is not on a default PATH even once emsdk is installed: its own installer
# tells you to source a script, and forgetting to is by far the most likely way
# to arrive here. So the failure says what to run rather than "command not
# found".
#
# EMSDK is honoured when it is set because that is what emsdk_env.sh exports,
# which makes `EMSDK=~/emsdk make web` work from a shell that has never sourced
# anything.
if ! command -v emcc >/dev/null 2>&1; then
    if [ -n "${EMSDK:-}" ] && [ -f "$EMSDK/emsdk_env.sh" ]; then
        # shellcheck disable=SC1091
        source "$EMSDK/emsdk_env.sh" >/dev/null 2>&1 || true
    elif [ -f "$HOME/emsdk/emsdk_env.sh" ]; then
        # shellcheck disable=SC1091
        source "$HOME/emsdk/emsdk_env.sh" >/dev/null 2>&1 || true
    fi
fi
if ! command -v emcc >/dev/null 2>&1; then
    cat >&2 <<'MSG'
build_web: no emcc on PATH.

    git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
    cd ~/emsdk && ./emsdk install latest && ./emsdk activate latest
    source ~/emsdk/emsdk_env.sh

emsdk needs a python 3.10 or newer to install itself; on a Mac whose python3 is
Xcode's, `EMSDK_PYTHON=$(brew --prefix)/bin/python3` is the fix.
MSG
    exit 1
fi

# SDL's own README-emscripten.md is blunt about this: it is possible to build
# against emscripten 3.x and "several things will be silently broken". Silently
# is the word that earns this check — the failure is not a compile error, it is
# a game that behaves oddly in a browser somebody else is using.
EMCC_MIN_MAJOR=4
emcc_version=$(emcc --version | sed -n '1s/.*clang-like replacement + linker emulating GNU ld) \([0-9][0-9.]*\).*/\1/p')
if [ -z "$emcc_version" ]; then
    emcc_version=$(emcc -dumpversion 2>/dev/null || echo 0)
fi
if [ "${emcc_version%%.*}" -lt "$EMCC_MIN_MAJOR" ] 2>/dev/null; then
    echo "build_web: emcc $emcc_version is older than $EMCC_MIN_MAJOR.0;" \
         "SDL documents 3.x as silently broken. Run: emsdk install latest" >&2
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "build_web: no cmake on PATH (SDL is configured with it)" >&2
    echo "           macOS: brew install cmake" >&2
    exit 1
fi

sdl_version=$(sed -n 's/^SDL_VERSION="\(.*\)"$/\1/p' "$root/packaging/fetch_sdl3.sh")
if [ -z "$sdl_version" ]; then
    echo "build_web: could not read SDL_VERSION from packaging/fetch_sdl3.sh" >&2
    exit 1
fi
# The hash of the artifact *this* script fetches. The version has one home and
# is read from it; a source tarball and a disk image are two different files
# and cannot share a checksum, so each fetch carries its own.
SDL_SRC_SHA256="30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb"

define() { sed -n "s/^#define $1 \"\(.*\)\"/\1/p" "$root/src/version.h"; }
version=$(define CHUCK_VERSION)
app_name=$(define CHUCK_APP_NAME)

# The frame the game draws, read off the header rather than written here. A
# resolution spelled in a shell script is a second copy of a #define, which is
# the failure tools/soak.sh was already corrected for.
config() { sed -n "s/^#define $1 \([0-9][0-9]*\).*/\1/p" "$root/src/game_config.h"; }
view_w=$(config VIEW_W)
view_h=$(config VIEW_H)
if [ -z "$view_w" ] || [ -z "$view_h" ]; then
    echo "build_web: could not read VIEW_W/VIEW_H from src/game_config.h" >&2
    exit 1
fi

payload=$app_name-$version-web
stage=$dist/stage/$payload

# ---- SDL, once per pin ----------------------------------------------------
sdl_src=$root/build/SDL3-$sdl_version
sdl_prefix=$root/build/sdl-web
if [ ! -f "$sdl_prefix/lib/libSDL3.a" ]; then
    archive=$root/build/SDL3-$sdl_version.tar.gz
    url=https://github.com/libsdl-org/SDL/releases/download/release-$sdl_version/SDL3-$sdl_version.tar.gz
    mkdir -p "$root/build"
    if [ ! -f "$archive" ]; then
        echo "build_web: downloading SDL $sdl_version (source)"
        curl --fail --location --progress-bar --output "$archive.part" "$url"
        mv "$archive.part" "$archive"
    fi
    have=$( (shasum -a 256 "$archive" 2>/dev/null || sha256sum "$archive") | cut -d' ' -f1)
    if [ "$have" != "$SDL_SRC_SHA256" ]; then
        echo "build_web: SDL source checksum mismatch" >&2
        echo "           expected $SDL_SRC_SHA256" >&2
        echo "           got      $have" >&2
        exit 1
    fi
    rm -rf "$sdl_src"
    tar -C "$root/build" -xzf "$archive"

    #
    # SDL_EMSCRIPTEN_PERSISTENT_PATH is the whole reason this is a source build.
    #
    # A browser tab's filesystem is RAM: write a file, close the tab, the file
    # is gone. This game keeps two of them — settings.cfg and progress.cfg,
    # under SDL_GetPrefPath — and without this option every player would lose
    # their controls, their volume and the whole climb every time they closed
    # the page, silently, because saving *works*: it is only the disk that
    # stops existing.
    #
    # Given the option, SDL mounts the browser's IndexedDB at this path, waits
    # for it to sync before it calls SDL_AppInit, and hands SDL_GetPrefPath a
    # directory underneath it. So the game's own save code is untouched — the
    # six call sites in game.c that ask pref_file_path for a path get one, and
    # what they write survives. Verified rather than assumed: a file written
    # into /storage/rob/Chuck in one page load reads back in the next.
    #
    # Shared libraries mean nothing here and the tests and examples are a
    # couple of minutes of compiling nobody is going to run, so both are off.
    #
    cmake_build=$root/build/sdl-web-build
    rm -rf "$cmake_build"
    mkdir -p "$cmake_build"
    ( cd "$cmake_build" && emcmake cmake "$sdl_src" \
        -DCMAKE_BUILD_TYPE=Release \
        -DSDL_EMSCRIPTEN_PERSISTENT_PATH=/storage \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON \
        -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF \
        -DCMAKE_INSTALL_PREFIX="$sdl_prefix" >/dev/null )
    echo "build_web: compiling SDL $sdl_version for wasm"
    cmake --build "$cmake_build" --parallel "$jobs" --target install >/dev/null
fi
if [ ! -f "$sdl_prefix/lib/libSDL3.a" ]; then
    echo "build_web: SDL did not install to $sdl_prefix" >&2
    exit 1
fi
# The persistent path is a compile-time decision and this is the one place it
# leaves a trace: SDL's own pkg-config puts -lidbfs.js in Libs when it has been
# built with one. Checking it here rather than trusting the cmake line is what
# stops a stale build/sdl-web from an older invocation being linked in silently
# — which would be a game that saves nothing and says so nowhere.
if ! grep -q -- "-lidbfs.js" "$sdl_prefix/lib/pkgconfig/sdl3.pc"; then
    echo "build_web: the SDL in $sdl_prefix was built without a persistent" >&2
    echo "           path, so nobody's settings or progress would survive" >&2
    echo "           closing the tab. Remove that directory and run again." >&2
    exit 1
fi

# ---- the game -------------------------------------------------------------
#
# The same Makefile the other three use, with emcc as the compiler and a .js
# where the executable goes — emcc reads the output extension and produces
# chuck.js beside chuck.wasm.
#
# The link flags, and why each is there:
#
#   ALLOW_MEMORY_GROWTH  SDL's README asks for it for anything of any size.
#   MAXIMUM_MEMORY=1gb   The ceiling that growth is allowed to reach.
#   INVOKE_RUN=0         Hold main back until the page's PLAY button is pressed,
#                        so that the first frame drawn is also the first frame
#                        allowed to make a sound. A browser silently discards
#                        audio until the page has been clicked on, and SDL will
#                        open a device and feed a muted one for as long as that
#                        lasts — which is a title screen whose attract music
#                        never arrives.
#   EXPORTED_RUNTIME_METHODS=callMain
#                        The one runtime method the page needs, and the thing
#                        INVOKE_RUN=0 is useless without.
#   -lidbfs.js           The Javascript half of the persistent path above.
#
# There are deliberately no pthreads. Threads on the web need COOP/COEP headers
# on the server, itch.io does not set them, and a build that needs them does not
# start there at all — it is not a slow path, it is a blank page. Nothing in
# src/ starts a thread; the one SDL_CreateThread in this repository is the
# editor's build button, and the editor is not a thing a player is handed.
make -C "$root" -j"$jobs" \
    CC=emcc \
    BUILD_DIR=build/web TARGET=build/web/chuck.js \
    CFLAGS="-std=c17 -Wall -Wextra -Wpedantic -O2 -I$sdl_prefix/include" \
    LDFLAGS="-L$sdl_prefix/lib -lSDL3 -lidbfs.js -lm \
             -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=1gb \
             -sINVOKE_RUN=0 -sEXPORTED_RUNTIME_METHODS=callMain" \
    all

# ---- the payload ----------------------------------------------------------
#
# index.html at the root, and nothing above it. See the note at the top.
rm -rf "$stage"
mkdir -p "$stage"
cp "$root/build/web/chuck.js" "$stage/chuck.js"
cp "$root/build/web/chuck.wasm" "$stage/chuck.wasm"

sed -e "s|@CHUCK_TITLE@|$app_name|g" \
    -e "s|@CHUCK_NAME_UPPER@|$(echo "$app_name" | tr '[:lower:]' '[:upper:]')|g" \
    -e "s|@VIEW_W@|$view_w|g" \
    -e "s|@VIEW_H@|$view_h|g" \
    -e "s|@CHUCK_JS@|chuck.js|g" \
    "$root/packaging/web_shell.html" >"$stage/index.html"

# A placeholder that survived is a page with `@VIEW_W@` in its Javascript,
# which is a blank canvas and a console error nobody is going to read.
if grep -q "@[A-Z_]\+@" "$stage/index.html"; then
    echo "build_web: unfilled placeholder in index.html:" >&2
    grep -o "@[A-Z_]\+@" "$stage/index.html" | sort -u >&2
    exit 1
fi

# No .itch.toml. That file names which of several files the itch.io *app* should
# launch, and there is no app here and no choice to make: a browser build is one
# page and the shop already knows its name. The macOS archive omits it for the
# same shape of reason.

archive=$dist/$payload.zip
mkdir -p "$dist"
rm -f "$archive"
# Zipped from inside the staging directory rather than above it, which is the
# whole difference from its three siblings: what has to be at the top of this
# archive is index.html and not a folder containing it.
(cd "$stage" && zip -q -r "$archive" .)
# The staging tree is an intermediate, not an artifact. Left standing it makes
# dist/ hold the payload twice — the archive and a loose copy of everything in
# it — which is how somebody comes to upload the folder instead of the zip.
rm -rf "$dist/stage"
echo "build_web: $archive ($(du -h "$archive" | cut -f1)) — upload this," \
     "and tick \"This file will be played in the browser\""
