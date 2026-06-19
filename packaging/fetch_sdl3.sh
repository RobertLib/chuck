#!/bin/bash
#
# Put the official universal SDL3.framework in vendor/, for the shipped app to
# be built and to ship against.
#
# The development build links Homebrew's SDL3, which is right for a machine
# that has Homebrew on it and wrong for everybody else: it is arm64 only and
# it is built for the macOS it was poured on (minos 26.0 as this was written),
# so a bundle wrapped around it runs on this Mac and refuses to start on any
# other. libsdl.org's own release carries both slices and a macOS 11 floor,
# which is the difference between a build and a build somebody else can run.
#
# The version and its hash are pinned here: a shipped binary must be traceable
# to the exact library it was linked against, and "whatever the latest release
# was on the day" is not that.
set -euo pipefail

SDL_VERSION="3.4.14"
SDL_SHA256="bae77509ccddcc7a443bb09730ab854c976e8f8bcf57b66d6bad6af2e17f38c2"
SDL_URL="https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL3-${SDL_VERSION}.dmg"

root=$(cd "$(dirname "$0")/.." && pwd)
vendor=${1:-$root/vendor}
framework="$vendor/SDL3.framework"

if [ -d "$framework" ]; then
    echo "vendor: SDL3.framework already present ($framework)"
    exit 0
fi

mkdir -p "$vendor"
dmg="$root/build/SDL3-${SDL_VERSION}.dmg"
mkdir -p "$root/build"

if [ ! -f "$dmg" ]; then
    echo "vendor: downloading SDL $SDL_VERSION"
    curl --fail --location --progress-bar --output "$dmg.part" "$SDL_URL"
    mv "$dmg.part" "$dmg"
fi

echo "vendor: verifying"
have=$(shasum -a 256 "$dmg" | awk '{print $1}')
if [ "$have" != "$SDL_SHA256" ]; then
    echo "vendor: sha256 mismatch for $dmg" >&2
    echo "  expected $SDL_SHA256" >&2
    echo "  got      $have" >&2
    exit 1
fi

mount=$(mktemp -d "${TMPDIR:-/tmp}/sdl3-mount.XXXXXX")
cleanup() { hdiutil detach -quiet "$mount" >/dev/null 2>&1 || true; rmdir "$mount" 2>/dev/null || true; }
trap cleanup EXIT

hdiutil attach -nobrowse -quiet "$dmg" -mountpoint "$mount"
src="$mount/SDL3.xcframework/macos-arm64_x86_64/SDL3.framework"
if [ ! -d "$src" ]; then
    echo "vendor: no macOS slice in SDL3.xcframework" >&2
    exit 1
fi

# ditto rather than cp: a framework is symlinks (Versions/Current, Headers,
# the binary), and a copy that flattens them is not a framework any more.
ditto "$src" "$framework"
echo "vendor: $framework ($(lipo -archs "$framework/SDL3"), minos $(otool -l "$framework/SDL3" | awk '/minos/{print $2; exit}'))"
