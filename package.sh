#!/bin/bash
# Build a Developer ID signed, notarized macOS installer without changing the running app.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ "${1:-}" == --help ]]; then
    echo 'Usage: ./package.sh [--skip-build]'
    echo 'Environment: BUILD_DIR, DIST_DIR, CODESIGN_IDENTITY, INSTALLER_IDENTITY, NOTARY_PROFILE, JOBS'
    echo 'Uses the build directory architecture; a new build defaults to this Mac architecture.'
    exit 0
fi
if [[ $# -gt 1 || ( $# -eq 1 && "$1" != --skip-build ) ]]; then
    echo 'Usage: ./package.sh [--skip-build]' >&2
    exit 2
fi
[[ "$(uname -s)" == Darwin ]] || { echo 'macOS is required.' >&2; exit 1; }
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
DIST_DIR="${DIST_DIR:-$ROOT/dist}"
CODESIGN_IDENTITY="${CODESIGN_IDENTITY:-Developer ID Application: Márton Ferenczi (Y8272MS92K)}"
INSTALLER_IDENTITY="${INSTALLER_IDENTITY:-Developer ID Installer: Márton Ferenczi (Y8272MS92K)}"
NOTARY_PROFILE="${NOTARY_PROFILE:-moth-notary}"

if [[ "${1:-}" != --skip-build ]]; then
    cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$BUILD_DIR" --config Release --target MothDSPLab -j "${JOBS:-6}"
fi
APP_SOURCE="$BUILD_DIR/MothDSPLab_artefacts/Release/Moth DSP Lab.app"
[[ -d "$APP_SOURCE" ]] || { echo "Missing Release app: $APP_SOURCE" >&2; exit 1; }
VERSION="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$APP_SOURCE/Contents/Info.plist")"
[[ "$VERSION" =~ ^[0-9]+(\.[0-9]+)*$ ]] || { echo 'Invalid bundle version.' >&2; exit 1; }
mkdir -p "$DIST_DIR"
DIST_DIR="$(cd "$DIST_DIR" && pwd)"
STAGING="$(mktemp -d "$DIST_DIR/.package.XXXXXX")"
trap 'rm -rf "$STAGING"' EXIT
APP="$STAGING/root/Applications/Moth DSP Lab.app"
mkdir -p "$STAGING/root/Applications"
ditto "$APP_SOURCE" "$APP"
xattr -cr "$APP"

# Audio capture needs audio-input under hardened runtime. Library validation must
# permit the user's third-party AU/VST3 plugins, signed by other developers.
codesign --force --options runtime --timestamp \
    --entitlements "$ROOT/scripts/mac-entitlements.plist" \
    --sign "$CODESIGN_IDENTITY" "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"
lipo -archs "$APP/Contents/MacOS/Moth DSP Lab"

# Disable bundle relocation: an existing development copy must never become the
# installation destination instead of /Applications.
pkgbuild --analyze --root "$STAGING/root" "$STAGING/components.plist"
/usr/libexec/PlistBuddy -c 'Set :0:BundleIsRelocatable false' "$STAGING/components.plist"
pkgbuild --root "$STAGING/root" --component-plist "$STAGING/components.plist" \
    --identifier hu.mothproduction.dsplab --version "$VERSION" \
    --install-location / "$STAGING/component.pkg"
PKG="$STAGING/Moth-DSP-Lab-$VERSION.pkg"
productbuild --package "$STAGING/component.pkg" --sign "$INSTALLER_IDENTITY" "$PKG"
pkgutil --check-signature "$PKG"

# Keep submission evidence even on failure; never publish a rejected package as ready.
LOG="$DIST_DIR/notarization-$VERSION-$(date +%Y%m%d-%H%M%S)-$$.json"
xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait --output-format json > "$LOG"
STATUS="$(plutil -extract status raw -o - "$LOG")"
[[ "$STATUS" == Accepted ]] || { echo "Notarization failed; see $LOG" >&2; exit 1; }
xcrun stapler staple "$PKG"
xcrun stapler validate "$PKG"
spctl --assess --type install --verbose=4 "$PKG"
OUTPUT="$DIST_DIR/Moth-DSP-Lab-$VERSION.pkg"
mv -f "$PKG" "$OUTPUT"
shasum -a 256 "$OUTPUT" > "$OUTPUT.sha256"
echo "Signed, notarized installer ready: $OUTPUT"
