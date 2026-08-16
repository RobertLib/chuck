#!/bin/bash
#
# Assemble dist/Chuck.app around an already-built universal binary, and sign it.
#
# Everything the app needs is inside it: the levels are in the executable, the
# audio is synthesized at startup, and SDL travels in Contents/Frameworks. The
# one thing a user's Mac has to supply is macOS itself.
#
# Signing is not a separate step you can forget. An unsigned or ad-hoc app is
# exactly what Gatekeeper stops, so this script always signs — with the
# Developer ID certificate if the keychain has one, ad-hoc if it does not, and
# it says which of the two it did.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
exe_in=${1:-$root/build/app/chuck}
dist=${DIST_DIR:-$root/dist}
vendor=${VENDOR_DIR:-$root/vendor}
min_version=${MACOS_MIN_VERSION:-11.0}

define() { sed -n "s/^#define $1 \"\(.*\)\"/\1/p" "$root/src/version.h"; }
version=$(define CHUCK_VERSION)
app_name=$(define CHUCK_APP_NAME)
bundle_id=${BUNDLE_ID:-$(define CHUCK_APP_ID)}
# CFBundleVersion has to rise with every build you hand anybody; the commit
# count is a number that already does.
build_number=$(git -C "$root" rev-list --count HEAD 2>/dev/null || echo 1)
copyright="Copyright © 2026 Robert Libšanský. MIT licensed."

[ -x "$exe_in" ] || { echo "build_app: no binary at $exe_in (run: make app)" >&2; exit 1; }
[ -d "$vendor/SDL3.framework" ] || { echo "build_app: no $vendor/SDL3.framework (run: packaging/fetch_sdl3.sh)" >&2; exit 1; }

app="$dist/$app_name.app"
contents="$app/Contents"
rm -rf "$app"
mkdir -p "$contents/MacOS" "$contents/Frameworks" "$contents/Resources"

# --- the executable --------------------------------------------------------
cp "$exe_in" "$contents/MacOS/chuck"
chmod 755 "$contents/MacOS/chuck"
strip -x "$contents/MacOS/chuck"

# --- SDL, travelling inside the bundle -------------------------------------
# The framework's install name is @rpath/SDL3.framework/Versions/A/SDL3 and the
# link step already wrote @executable_path/../Frameworks into the binary, so
# there is nothing to rewrite here — the bundle is simply where the rpath says
# to look. Headers are dropped: three and a half megabytes of .h files are not
# something a player needs a copy of.
ditto "$vendor/SDL3.framework" "$contents/Frameworks/SDL3.framework"
rm -rf "$contents/Frameworks/SDL3.framework/Headers" \
       "$contents/Frameworks/SDL3.framework/Versions/A/Headers" \
       "$contents/Frameworks/SDL3.framework/Versions/A/Resources/CMake"
# Its own signature sealed the files just removed, so it has to go with them.
rm -rf "$contents/Frameworks/SDL3.framework/Versions/A/_CodeSignature"

# --- the wrapping ----------------------------------------------------------
"$root/packaging/make_icns.sh" "$contents/Resources/$app_name.icns"

sed -e "s|@NAME@|$app_name|g" \
    -e "s|@EXECUTABLE@|chuck|g" \
    -e "s|@ICON@|$app_name|g" \
    -e "s|@BUNDLE_ID@|$bundle_id|g" \
    -e "s|@VERSION@|$version|g" \
    -e "s|@BUILD@|$build_number|g" \
    -e "s|@MIN_VERSION@|$min_version|g" \
    -e "s|@COPYRIGHT@|$copyright|g" \
    "$root/packaging/Info.plist.in" > "$contents/Info.plist"
printf 'APPL????' > "$contents/PkgInfo"

# --- signing ---------------------------------------------------------------
identity=${CODESIGN_IDENTITY:-}
if [ -z "$identity" ]; then
    # A Mac signed into two teams has two Developer ID certificates, and
    # silently taking the first one ships a build under whichever name sorted
    # earliest. Say which ones there are and let CODESIGN_IDENTITY decide.
    found=$(security find-identity -v -p codesigning 2>/dev/null \
        | sed -n 's/.*"\(Developer ID Application: [^"]*\)".*/\1/p')
    count=$(printf '%s' "$found" | grep -c . || true)
    if [ "$count" -gt 1 ]; then
        echo "sign: more than one Developer ID Application certificate:"
        printf '%s\n' "$found" | sed 's/^/        /'
        echo "      pick one with: CODESIGN_IDENTITY=\"...\" make app"
        exit 1
    fi
    identity=$found
fi

if [ -n "$identity" ]; then
    # --options runtime is the hardened runtime, and notarization requires it.
    # --timestamp asks Apple's timestamp server, which is what keeps the
    # signature valid after the certificate itself expires.
    sign=(codesign --force --timestamp --options runtime --sign "$identity")
    echo "sign: $identity"
else
    # Ad-hoc: enough to run on this machine, never enough to notarize. Said
    # plainly, because a build that Gatekeeper will refuse must not look like a
    # build that succeeded.
    sign=(codesign --force --sign -)
    echo "sign: AD-HOC — no Developer ID Application certificate in the keychain."
    echo "      The bundle will run here and be refused on any other Mac."
fi

"${sign[@]}" "$contents/Frameworks/SDL3.framework/Versions/A"
"${sign[@]}" "$app"

codesign --verify --strict --verbose=2 "$app" 2>&1 | sed 's/^/verify: /'

# --- what came out ---------------------------------------------------------
echo
echo "$app"
echo "  version   $version ($build_number), bundle id $bundle_id"
echo "  arch      $(lipo -archs "$contents/MacOS/chuck")"
echo "  min macOS $min_version"
echo "  size      $(du -sh "$app" | awk '{print $1}')"
if [ -n "$identity" ]; then
    echo
    echo "Next: packaging/notarize.sh (or make notarize)"
fi
