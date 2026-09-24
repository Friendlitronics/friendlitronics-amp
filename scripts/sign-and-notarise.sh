#!/usr/bin/env bash
#
# Signs, notarises and staples the built plugins, then packages them.
#
# The result installs with no Gatekeeper prompt and no Terminal commands — the
# difference between "here is a zip, now run this incantation" and "here is a
# plugin". Everything below needs a paid Apple Developer account.
#
# Required environment:
#   SIGN_ID       "Developer ID Application: Your Name (TEAMID)"
#   TEAM_ID       your 10-character Apple team id
# Authentication, either:
#   APPLE_ID + APP_PASSWORD      (an app-specific password, not your real one)
# or:
#   KEYCHAIN_PROFILE             a profile stored with `notarytool store-credentials`
#
# Usage:  scripts/sign-and-notarise.sh [build-dir]
#
set -euo pipefail

BUILD_DIR="${1:-build}"
ARTEFACTS="$BUILD_DIR/FriendlitronicsAmp_artefacts/Release"
VERSION=$(grep -m1 'project(FriendlitronicsAmp VERSION' CMakeLists.txt | sed -E 's/.*VERSION ([0-9.]+).*/\1/')
STAGE="dist/FriendlitronicsAmp-${VERSION}-macOS"
ZIP="${STAGE}.zip"

: "${SIGN_ID:?set SIGN_ID to your Developer ID Application identity}"
: "${TEAM_ID:?set TEAM_ID to your Apple team id}"

VST3="$ARTEFACTS/VST3/Friendlitronics Amp.vst3"
AU="$ARTEFACTS/AU/Friendlitronics Amp.component"

for b in "$VST3" "$AU"; do
    [ -d "$b" ] || { echo "missing build output: $b" >&2; exit 1; }
done

echo "==> signing"
for b in "$VST3" "$AU"; do
    # --options runtime is what makes the bundle eligible for notarisation;
    # without the hardened runtime Apple rejects the submission.
    codesign --force --deep --strict --timestamp --options runtime \
             --sign "$SIGN_ID" "$b"
    codesign --verify --deep --strict --verbose=2 "$b"
done

echo "==> staging"
rm -rf "$STAGE" "$ZIP"
mkdir -p "$STAGE"
ditto "$VST3" "$STAGE/Friendlitronics Amp.vst3"
ditto "$AU"   "$STAGE/Friendlitronics Amp.component"
cp docs/INSTALL-macOS.txt "$STAGE/How to install.txt"
ditto -c -k --sequesterRsrc --keepParent "$STAGE" "$ZIP"

echo "==> notarising (this waits on Apple, usually a few minutes)"
if [ -n "${KEYCHAIN_PROFILE:-}" ]; then
    xcrun notarytool submit "$ZIP" --keychain-profile "$KEYCHAIN_PROFILE" --wait
else
    : "${APPLE_ID:?set APPLE_ID, or use KEYCHAIN_PROFILE}"
    : "${APP_PASSWORD:?set APP_PASSWORD to an app-specific password}"
    xcrun notarytool submit "$ZIP" --apple-id "$APPLE_ID" \
          --team-id "$TEAM_ID" --password "$APP_PASSWORD" --wait
fi

echo "==> stapling"
# Staple the bundles, not the zip, so the ticket travels with the plugin even
# if someone re-compresses it. Then rebuild the zip from the stapled copies.
for b in "$STAGE/Friendlitronics Amp.vst3" "$STAGE/Friendlitronics Amp.component"; do
    xcrun stapler staple "$b"
    xcrun stapler validate "$b"
done

rm -f "$ZIP"
ditto -c -k --sequesterRsrc --keepParent "$STAGE" "$ZIP"

echo
echo "done: $ZIP"
echo "This build installs with no quarantine step. Verify on another Mac with:"
echo "  spctl -a -vvv -t install \"$STAGE/Friendlitronics Amp.component\""
