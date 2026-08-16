#!/bin/bash
#
# Notarize dist/Chuck.app and hand back a DMG anybody can open.
#
# Notarization is Apple scanning the signed build and issuing a ticket for it;
# stapling writes that ticket into the bundle so the first launch needs no
# network. Both the .app and the .dmg are notarized and stapled, because the
# player may double-click either one and Gatekeeper checks whichever they got.
#
# One-time setup (Xcode's notarytool keeps the credentials in the keychain):
#
#   xcrun notarytool store-credentials chuck-notary \
#       --apple-id you@example.com --team-id XXXXXXXXXX \
#       --password <app-specific-password from appleid.apple.com>
#
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
dist=${DIST_DIR:-$root/dist}
profile=${NOTARY_PROFILE:-chuck-notary}

define() { sed -n "s/^#define $1 \"\(.*\)\"/\1/p" "$root/src/version.h"; }
version=$(define CHUCK_VERSION)
app_name=$(define CHUCK_APP_NAME)
app="$dist/$app_name.app"

[ -d "$app" ] || { echo "notarize: no $app (run: make app)" >&2; exit 1; }

# Apple rejects anything not signed with a Developer ID Application
# certificate, and it rejects it after the upload rather than before it. Fail
# here instead, where the message can say what to do about it.
identity=$(codesign -dvv "$app" 2>&1 | sed -n 's/^Authority=\(Developer ID Application.*\)/\1/p' | head -1)
if [ -z "$identity" ]; then
    cat >&2 <<'MSG'
notarize: dist/Chuck.app is not signed with a Developer ID Application certificate.

Apple only notarizes builds signed with one, and it is not the "Apple
Development" certificate Xcode makes for running on your own devices. It needs
a paid Apple Developer Program membership, and it is created in
Xcode > Settings > Accounts > (your team) > Manage Certificates > + >
Developer ID Application — or on developer.apple.com under Certificates.

Once it is in the keychain, run: make app && make notarize
MSG
    exit 1
fi
echo "notarize: signed by $identity"
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

notarize: no credentials stored under the profile "$profile".

Store them once — the password is an app-specific password, made at
appleid.apple.com > Sign-In and Security > App-Specific Passwords, and not
your Apple ID password:

  xcrun notarytool store-credentials $profile \\
      --apple-id <your-apple-id-email> \\
      --team-id $team_id \\
      --password <app-specific-password>

Then: make notarize
MSG
        rm -f "$out"
        return 1
    fi

    local id
    id=$(sed -n 's/^ *id: \([0-9a-f-]*\)$/\1/p' "$out" | head -1)
    if [ -n "$id" ]; then
        echo
        echo "notarize: Apple refused it. The reasons:"
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
echo "notarize: submitting $app_name.app"
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
echo "notarize: submitting $(basename "$dmg")"
submit "$dmg"
xcrun stapler staple "$dmg"

# --- what Gatekeeper now says ----------------------------------------------
echo
spctl --assess --type exec --verbose=2 "$app" 2>&1 | sed 's/^/gatekeeper: /'
spctl --assess --type open --context context:primary-signature --verbose=2 "$dmg" 2>&1 | sed 's/^/gatekeeper: /'
xcrun stapler validate "$app" | sed 's/^/staple: /'
xcrun stapler validate "$dmg" | sed 's/^/staple: /'

echo
echo "$dmg"
echo "  $(du -sh "$dmg" | awk '{print $1}') — notarized, stapled, ready to hand out."
