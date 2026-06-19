#!/bin/bash
#
# The macOS build, end to end: compile both slices, wrap them in dist/Chuck.app,
# sign it, get it a notarization ticket from Apple, staple that in, and cut the
# two archives a player can be handed — a zip and a DMG.
#
# One script per platform, beside build_linux.sh and build_windows.sh, and it
# does what those two do in the same order: the library, the game, the payload,
# the archive. macOS had it in two files under three Make targets, with a fourth
# naming the order they had to run in, and the split is what shipped an archive
# nobody could open — the step that cut the zip depended on the step that rebuilds
# and re-signs the bundle, so it threw away the ticket it needed and packed the
# result. A platform whose release is one script cannot be run in the wrong order.
#
# `MACOS_BUNDLE_ONLY=1` stops after signing, which is what `make app` and the
# macOS CI job use: everything up to that point needs no Apple account, so it can
# be checked on every push, and everything after it needs the network and a
# notary profile.
#
# Everything the app needs is inside it: the levels are in the executable, the
# audio is synthesized at startup, and SDL travels in Contents/Frameworks. The
# one thing a user's Mac has to supply is macOS itself.
#
# Signing is not a step you can forget. An unsigned or ad-hoc app is exactly what
# Gatekeeper stops, so this always signs — with the Developer ID certificate if
# the keychain has one, ad-hoc if it does not, and it says which of the two it
# did. Notarization is Apple scanning the signed build and issuing a ticket;
# stapling writes that ticket into the bundle so the first launch needs no
# network. Both the .app and the .dmg are notarized and stapled, because the
# player may double-click either and Gatekeeper checks whichever they got.
#
# One-time setup for the notarizing half (notarytool keeps the credentials in the
# keychain):
#
#   xcrun notarytool store-credentials chuck-notary \
#       --apple-id you@example.com --team-id XXXXXXXXXX \
#       --password <app-specific-password from appleid.apple.com>
#
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
dist=${DIST_DIR:-$root/dist}
vendor=${VENDOR_DIR:-$root/vendor}
min_version=${MACOS_MIN_VERSION:-11.0}
profile=${NOTARY_PROFILE:-chuck-notary}
jobs=${JOBS:-$( (sysctl -n hw.ncpu 2>/dev/null || echo 4) )}
bundle_only=${MACOS_BUNDLE_ONLY:-0}

define() { sed -n "s/^#define $1 \"\(.*\)\"/\1/p" "$root/src/version.h"; }
version=$(define CHUCK_VERSION)
app_name=$(define CHUCK_APP_NAME)
bundle_id=${BUNDLE_ID:-$(define CHUCK_APP_ID)}
# CFBundleVersion has to rise with every build you hand anybody; the commit
# count is a number that already does.
build_number=$(git -C "$root" rev-list --count HEAD 2>/dev/null || echo 1)
copyright="Copyright © 2026 Robert Libšanský. MIT licensed."

# ---- SDL, once per pin ----------------------------------------------------
# `make` links Homebrew's SDL3, which is right for this machine and wrong for
# everyone else's: arm64 only, and built for the macOS it was poured on. The
# bundle is built against libsdl.org's own universal framework instead, fetched
# with its version and sha256 pinned in the script.
[ -d "$vendor/SDL3.framework" ] || "$root/packaging/fetch_sdl3.sh" "$vendor"

# ---- the game, both slices ------------------------------------------------
exe=$root/build/app/chuck
MACOSX_DEPLOYMENT_TARGET=$min_version make -C "$root" -j"$jobs" \
    BUILD_DIR=build/app TARGET=build/app/chuck \
    CFLAGS="-std=c17 -Wall -Wextra -Wpedantic -O2 -arch arm64 -arch x86_64 -F$vendor" \
    LDFLAGS="-arch arm64 -arch x86_64 -F$vendor -framework SDL3 -lm \
        -Wl,-rpath,@executable_path/../Frameworks" \
    all

[ -x "$exe" ] || { echo "build_macos: no binary at $exe" >&2; exit 1; }

# ---- the bundle -----------------------------------------------------------
app="$dist/$app_name.app"
contents="$app/Contents"
rm -rf "$app"
mkdir -p "$contents/MacOS" "$contents/Frameworks" "$contents/Resources"

# --- the executable --------------------------------------------------------
cp "$exe" "$contents/MacOS/chuck"
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
        echo "      pick one with: CODESIGN_IDENTITY=\"...\" make mac"
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

# ---- what the bundle came out as -----------------------------------------
echo
echo "$app"
echo "  version   $version ($build_number), bundle id $bundle_id"
echo "  arch      $(lipo -archs "$contents/MacOS/chuck")"
echo "  min macOS $min_version"
echo "  size      $(du -sh "$app" | awk '{print $1}')"

if [ "$bundle_only" = "1" ]; then
    echo
    echo "build_macos: MACOS_BUNDLE_ONLY=1 — stopping before Apple."
    # Said out loud, because this run has just deleted and re-signed the bundle
    # and any notarization ticket went with it. That is the whole of the bug this
    # script was merged to make impossible, in the one place it can still happen:
    # a `make app` on a machine that had a finished release sitting in dist/
    # leaves an archive beside a bundle that no longer matches it.
    echo "             $app is signed and NOT notarized — Gatekeeper will"
    echo "             refuse it anywhere but here. \`make mac\` is the release."
    exit 0
fi
if [ -z "$identity" ]; then
    cat >&2 <<'MSG'

build_macos: cannot notarize an ad-hoc signature.

Apple only notarizes builds signed with a Developer ID Application certificate,
and that is not the "Apple Development" certificate Xcode makes for running on
your own devices. It needs a paid Apple Developer Program membership, and it is
created in Xcode > Settings > Accounts > (your team) > Manage Certificates > + >
Developer ID Application — or on developer.apple.com under Certificates.

MACOS_BUNDLE_ONLY=1 builds and signs without going near Apple.
MSG
    exit 1
fi
team_id=$(printf '%s' "$identity" | sed -n 's/.*(\([A-Z0-9]*\))$/\1/p')

submit() {
    # notarytool --wait exits non-zero when the ticket is refused, but the
    # reason is only in the log, so fetch it rather than leaving a bare failure.
    local what=$1 out
    out=$(mktemp "${TMPDIR:-/tmp}/notary.XXXXXX")
    if xcrun notarytool submit "$what" --keychain-profile "$profile" --wait 2>&1 | tee "$out"; then
        rm -f "$out"
        return 0
    fi

    # The credentials are a one-time keychain profile, and notarytool's own
    # message names the command without any of the arguments it needs. The team
    # id is in the certificate we just signed with, so fill the line in.
    if grep -q "No Keychain password item found" "$out"; then
        cat <<MSG

build_macos: no credentials stored under the profile "$profile".

Store them once — the password is an app-specific password, made at
appleid.apple.com > Sign-In and Security > App-Specific Passwords, and not
your Apple ID password:

  xcrun notarytool store-credentials $profile \\
      --apple-id <your-apple-id-email> \\
      --team-id $team_id \\
      --password <app-specific-password>

Then: make mac
MSG
        rm -f "$out"
        return 1
    fi

    local id
    id=$(sed -n 's/^ *id: \([0-9a-f-]*\)$/\1/p' "$out" | head -1)
    if [ -n "$id" ]; then
        echo
        echo "build_macos: Apple refused it. The reasons:"
        xcrun notarytool log "$id" --keychain-profile "$profile" || true
    fi
    rm -f "$out"
    return 1
}

# --- the app ---------------------------------------------------------------
# A .app is a directory, and the upload has to be a single file: ditto's zip is
# the one that preserves the symlinks inside the framework and the signature
# with them.
zip="$dist/$app_name-$version.zip"
rm -f "$zip"
# It is an upload envelope, not an artifact: it goes whether or not the rest of
# this succeeds, so a failed run leaves dist/ holding only things worth having.
trap 'rm -f "$zip"' EXIT
ditto -c -k --keepParent "$app" "$zip"
echo "build_macos: submitting $app_name.app"
submit "$zip"
xcrun stapler staple "$app"

# --- the disk image --------------------------------------------------------
# Built from the stapled app, so what the player drags across already carries
# its ticket even before the DMG's own is checked.
staging="$dist/.dmg-staging"
rm -rf "$staging"
mkdir -p "$staging"
ditto "$app" "$staging/$app_name.app"
ln -s /Applications "$staging/Applications"

dmg="$dist/$app_name-$version.dmg"
rm -f "$dmg"
hdiutil create -volname "$app_name $version" -srcfolder "$staging" \
    -ov -format UDZO -quiet "$dmg"
rm -rf "$staging"

codesign --force --timestamp --sign "$identity" "$dmg"
echo "build_macos: submitting $(basename "$dmg")"
submit "$dmg"
xcrun stapler staple "$dmg"

# --- the archive -----------------------------------------------------------
#
# `ditto` rather than `zip`, for the reason packaging/fetch_sdl3.sh uses it: a
# `.app` is symlinks and resource forks, and an archiver that flattens either
# hands somebody a bundle that will not launch and cannot be diagnosed from the
# outside.
#
# The archive holds `Chuck.app` and nothing else — no launch manifest beside it,
# unlike the Linux and Windows payloads. A `.itch.toml` names which file in a
# folder is the game, which matters when a shared library is sitting next to a
# binary; a bundle is already the single obvious thing to double-click, and
# anything added *inside* it would be a file the code signature does not cover,
# which is to say a broken signature on the platform that refuses to start those.
archive="$dist/$app_name-$version-macos.zip"
rm -f "$archive"
ditto -c -k --sequesterRsrc --keepParent "$app" "$archive"

# --- what Gatekeeper now says ----------------------------------------------
echo
spctl --assess --type exec --verbose=2 "$app" 2>&1 | sed 's/^/gatekeeper: /'
spctl --assess --type open --context context:primary-signature --verbose=2 "$dmg" 2>&1 | sed 's/^/gatekeeper: /'
xcrun stapler validate "$app" | sed 's/^/staple: /'
xcrun stapler validate "$dmg" | sed 's/^/staple: /'

echo
echo "$archive"
echo "  $(du -sh "$archive" | awk '{print $1}') — notarized, stapled, upload this."
echo "$dmg"
echo "  $(du -sh "$dmg" | awk '{print $1}') — the same build as a disk image."
