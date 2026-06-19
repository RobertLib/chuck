#!/bin/bash
#
# Cross-build the Windows payload from a Unix machine: chuck.exe, SDL3.dll, and
# nothing to install.
#
# mingw-w64 rather than MSVC, for one reason: this tree is C17 built with
# `-Wall -Wextra -Wpedantic` and it links nothing but libc and SDL — there is no
# Windows in it anywhere. `src/*.c` includes SDL and the C standard library and
# stops, so what a Windows build needs is a compiler that targets Windows, and a
# gcc that does is a shorter path than a second project file. (The one file in
# the repository that would not cross this way is editor/editor_app.c, which
# includes <unistd.h> — and the editor is a tool, not something a player is
# handed.)
#
# SDL comes from libsdl.org's own mingw development release, at the same pin
# packaging/fetch_sdl3.sh holds and read out of that file rather than written
# down again: a shipped binary has to be traceable to the library it was linked
# against. That archive carries the import library and the DLL, and the DLL
# travels in the payload beside the exe — where Windows looks first — so the
# player installs nothing.
#
# `-static-libgcc` because the alternative is shipping libgcc_s_seh-1.dll for
# the sake of a compiler runtime nobody chose, and a missing one of those is a
# dialog box about a DLL rather than anything about this game. `-mwindows` so the
# process is a GUI application and no console window opens behind the game.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
dist=${DIST_DIR:-$root/dist}
triple=${MINGW_TRIPLE:-x86_64-w64-mingw32}
cc=${MINGW_CC:-$triple-gcc}
jobs=${JOBS:-$( (nproc 2>/dev/null || echo 4) )}

if ! command -v "$cc" >/dev/null 2>&1; then
    echo "build_windows: no $cc on PATH" >&2
    echo "               Debian/Ubuntu: apt install mingw-w64" >&2
    echo "               macOS: brew install mingw-w64" >&2
    exit 1
fi

sdl_version=$(sed -n 's/^SDL_VERSION="\(.*\)"$/\1/p' "$root/packaging/fetch_sdl3.sh")
if [ -z "$sdl_version" ]; then
    echo "build_windows: could not read SDL_VERSION from packaging/fetch_sdl3.sh" >&2
    exit 1
fi

define() { sed -n "s/^#define $1 \"\(.*\)\"/\1/p" "$root/src/version.h"; }
version=$(define CHUCK_VERSION)
app_name=$(define CHUCK_APP_NAME)
# The save directory is `SDL_GetPrefPath(CHUCK_APP_ORG, CHUCK_APP_NAME)`, and this
# README is the only place a player is told where it is — so it is read out of
# the header rather than typed here. It was typed here, which made the one
# sentence a player needs in order to delete their save a third copy of a name
# only version.h owns.
app_org=$(define CHUCK_APP_ORG)

# Staged under the name the archive is called, so unpacking it leaves one named
# folder rather than an exe and a DLL loose in a Downloads directory — which on
# Windows is the difference between the game finding SDL3.dll and not.
payload=$app_name-$version-windows-x64
stage=$dist/stage/$payload

# ---- SDL, once per pin ----------------------------------------------------
sdl_root=$root/build/SDL3-devel-$sdl_version-mingw
sdl_prefix=$sdl_root/SDL3-$sdl_version/$triple
if [ ! -f "$sdl_prefix/lib/libSDL3.dll.a" ]; then
    archive=$root/build/SDL3-devel-$sdl_version-mingw.tar.gz
    url=https://github.com/libsdl-org/SDL/releases/download/release-$sdl_version/SDL3-devel-$sdl_version-mingw.tar.gz
    mkdir -p "$root/build"
    if [ ! -f "$archive" ]; then
        echo "build_windows: downloading SDL $sdl_version (mingw)"
        curl --fail --location --progress-bar --output "$archive.part" "$url"
        mv "$archive.part" "$archive"
    fi
    rm -rf "$sdl_root"
    mkdir -p "$sdl_root"
    tar -C "$sdl_root" -xzf "$archive"
    if [ ! -f "$sdl_prefix/lib/libSDL3.dll.a" ]; then
        echo "build_windows: no $triple slice in $archive" >&2
        exit 1
    fi
fi

# ---- the icon, if this machine can draw one ------------------------------
# Optional on purpose. The icon is the one part of a Windows build that needs two
# more tools than the compiler, and an exe with the default icon is a cosmetic
# loss rather than a broken build — so it is skipped with a line saying so
# instead of failing the payload. It is drawn by the same script the macOS
# .icns comes from, which is why there is no .ico checked in: this repository
# holds no binary art.
# ImageMagick 7 calls itself `magick` and 6 calls itself `convert`, and which one
# a machine has is not a thing to have an opinion about: Ubuntu's `imagemagick`
# package — which is what the release job installs — is still 6, so a check for
# `magick` alone would have silently shipped every CI build with the default icon.
im=""
if command -v magick >/dev/null 2>&1; then
    im=magick
elif command -v convert >/dev/null 2>&1; then
    im=convert
fi

icon_object=""
if [ -n "$im" ] && command -v "$triple-windres" >/dev/null 2>&1; then
    icon_dir=$root/build/windows-icon
    mkdir -p "$icon_dir"
    python3 "$root/packaging/draw_icon.py" "$icon_dir/master.png"
    "$im" "$icon_dir/master.png" \
        -define icon:auto-resize=256,128,64,48,32,16 "$icon_dir/chuck.ico"
    printf '1 ICON "chuck.ico"\n' >"$icon_dir/icon.rc"
    (cd "$icon_dir" && "$triple-windres" icon.rc -O coff -o icon.o)
    icon_object=$icon_dir/icon.o
else
    echo "build_windows: no ImageMagick and/or $triple-windres;" \
         "exe gets the default icon"
fi

# ---- the game -------------------------------------------------------------
# The icon object rides in LDFLAGS because the link line puts those after the
# objects, and an .o among them links exactly as one of them does. It is the one
# thing here the Makefile has no variable for, and inventing one for a resource
# file only Windows has would put a platform into a file that has none.
make -C "$root" -j"$jobs" \
    CC="$cc" \
    BUILD_DIR=build/windows TARGET=build/windows/chuck.exe \
    CFLAGS="-std=c17 -Wall -Wextra -Wpedantic -O2 -I$sdl_prefix/include" \
    LDFLAGS="$icon_object -L$sdl_prefix/lib -lSDL3 -lm -mwindows -static-libgcc" \
    all

# ---- the payload ----------------------------------------------------------
rm -rf "$stage"
mkdir -p "$stage"
cp "$root/build/windows/chuck.exe" "$stage/chuck.exe"
cp "$sdl_prefix/bin/SDL3.dll" "$stage/SDL3.dll"
"$triple-strip" "$stage/chuck.exe" 2>/dev/null || true

"$root/packaging/itch_manifest.sh" "$stage" chuck.exe

cat >"$stage/README.txt" <<TXT
$app_name $version — Windows x64

Run chuck.exe

SDL3.dll is the library this build was made against; keep it next to the exe.
Nothing needs installing and nothing is written outside the folder below.

Settings and progress are kept in %APPDATA%\\$app_org\\$app_name. Deleting that folder
starts the tower again.

Windows may warn that the publisher is unknown: this build is not signed with
an Authenticode certificate. The source is at
https://github.com/RobertLib/chuck if you would rather build it yourself.
TXT

archive=$dist/$payload.zip
rm -f "$archive"
(cd "$dist/stage" && zip -q -r "$archive" "$payload")
# The staging tree is an intermediate, not an artifact. Left standing it makes
# dist/ hold the payload twice — the archive and a loose copy of everything in it
# — which is how somebody comes to upload the folder instead of the zip.
rm -rf "$dist/stage"
echo "build_windows: $archive ($(du -h "$archive" | cut -f1)) — upload this"
