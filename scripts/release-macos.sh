#!/bin/sh
# Builds, tests, signs and notarises the macOS disk image on this Mac, then staples the
# notarisation ticket to it. With --upload, it also attaches the disk image to the GitHub release
# tagged v<version>, replacing the ad hoc signed one from the release workflow.
#
#   sh scripts/release-macos.sh <version> [--upload]
#
# Needs a "Developer ID Application" certificate in the keychain and notarisation credentials
# stored once with:
#
#   xcrun notarytool store-credentials favdir --apple-id <Apple ID> --team-id <team ID>
#
# FAVDIR_CODESIGN_IDENTITY overrides the certificate (default: "Developer ID Application") and
# FAVDIR_NOTARY_PROFILE the credentials profile (default: favdir). The release workflow uses an
# App Store Connect API key instead of a profile: FAVDIR_NOTARY_KEY (path to the .p8 file),
# FAVDIR_NOTARY_KEY_ID and FAVDIR_NOTARY_ISSUER.

set -eu

usage() {
    echo "usage: sh scripts/release-macos.sh <version> [--upload]" >&2
    exit 2
}

[ $# -ge 1 ] || usage
VERSION=${1#v}
UPLOAD=0
if [ $# -ge 2 ]; then
    [ "$2" = "--upload" ] || usage
    UPLOAD=1
fi
if ! echo "$VERSION" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "Version '$1' is not X.Y.Z or vX.Y.Z" >&2
    exit 2
fi

IDENTITY=${FAVDIR_CODESIGN_IDENTITY:-Developer ID Application}
PROFILE=${FAVDIR_NOTARY_PROFILE:-favdir}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build/macos-release"
DMG="$BUILD/favdir-$VERSION-macos.dmg"

# notarytool with the API key when one is set, otherwise with the stored profile.
notary() {
    if [ -n "${FAVDIR_NOTARY_KEY:-}" ]; then
        xcrun notarytool "$@" --key "$FAVDIR_NOTARY_KEY" \
            --key-id "$FAVDIR_NOTARY_KEY_ID" --issuer "$FAVDIR_NOTARY_ISSUER"
    else
        xcrun notarytool "$@" --keychain-profile "$PROFILE"
    fi
}

# Check the certificate and the credentials before the build, not after it.
if ! security find-identity -v -p codesigning | grep -q "\"$IDENTITY"; then
    echo "No codesigning identity matching '$IDENTITY' in the keychain." >&2
    exit 1
fi
if ! CHECK=$(notary history 2>&1); then
    echo "$CHECK" >&2
    echo "The notarytool credentials do not work. For a profile, store them with:" >&2
    echo "  xcrun notarytool store-credentials $PROFILE --apple-id <Apple ID> --team-id <team ID>" >&2
    exit 1
fi

sh "$ROOT/scripts/setup.sh"

# A separate build folder, so the Developer ID setting does not reach everyday builds.
cmake --preset macos -B "$BUILD" \
    "-DFAVDIR_VERSION=$VERSION" "-DFAVDIR_CODESIGN_IDENTITY=$IDENTITY"
cmake --build "$BUILD" --config Release
ctest --test-dir "$BUILD" -C Release --output-on-failure

rm -f "$DMG"
(cd "$BUILD" && cpack -C Release)
codesign --sign "$IDENTITY" --timestamp "$DMG"

# notarytool can exit with 0 for a rejected submission, so check the status it prints.
RESULT=$(notary submit "$DMG" --wait)
echo "$RESULT"
if ! echo "$RESULT" | grep -q "status: Accepted"; then
    ID=$(echo "$RESULT" | awk '/^ *id:/ { print $2; exit }')
    echo "Notarisation failed. Apple's log:" >&2
    notary log "$ID" >&2 || true
    exit 1
fi
xcrun stapler staple "$DMG"
spctl --assess --type open --context context:primary-signature --verbose "$DMG"

echo "Notarised disk image: $DMG"

if [ "$UPLOAD" = 1 ]; then
    gh release upload "v$VERSION" "$DMG" --clobber
    echo "Attached to release v$VERSION"
fi
