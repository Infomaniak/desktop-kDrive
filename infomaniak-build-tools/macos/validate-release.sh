#!/usr/bin/env bash
# Fail before any release upload if the Swift release is incomplete or invalid.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# shellcheck source=infomaniak-build-tools/version-helpers.sh
source "$repo_root/infomaniak-build-tools/version-helpers.sh"
version="$(GetVersionFromJson "$repo_root" true macOS)"
install_dir="${INSTALL_DIR:-$repo_root/build-macos/client/install}"
artifacts_dir="${ARTIFACTS_DIR:-$repo_root/build-macos}"
app="$install_dir/kDrive.app"
client="$app/Contents/MacOS/kDrive.app"
updater="$install_dir/kDriveRecoveryUpdater.app"

require_file() {
    if [ ! -s "$1" ]; then
        echo "Error: required release artifact missing or empty: $1" >&2
        exit 1
    fi
}

for file in "kDrive-$version.pkg" "kDriveRecoveryUpdater-$version.zip" \
    "sparkle/kDrive-$version.zip" "sparkle/update-macos-$version.xml"; do
    require_file "$install_dir/$file"
done

check_symbols() {
    local binary="$1" dsym="$2"
    require_file "$binary"
    require_file "$dsym"
    lipo -verify_arch x86_64 arm64 "$binary"
    lipo -verify_arch x86_64 arm64 "$dsym"
    # dwarfdump includes the filename; compare UUIDs and architectures only.
    if [ "$(dwarfdump --uuid "$binary" | cut -d ' ' -f 1-3 | sort)" != \
         "$(dwarfdump --uuid "$dsym" | cut -d ' ' -f 1-3 | sort)" ]; then
        echo "Error: symbols do not match $binary" >&2
        exit 1
    fi
}

check_symbols "$app/Contents/MacOS/kDrive" "$artifacts_dir/sentry-server-artifacts/kDrive.dSYM/Contents/Resources/DWARF/kDrive"
check_symbols "$client/Contents/MacOS/kDrive.gui" "$artifacts_dir/sentry-client-artifacts/kDrive_client.dSYM/Contents/Resources/DWARF/kDrive.gui"
for framework in kDriveCore kDriveCoreUI kDriveResources; do
    check_symbols "$client/Contents/Frameworks/$framework.framework/Versions/A/$framework" \
        "$artifacts_dir/sentry-client-artifacts/client-frameworks/$framework.dSYM/Contents/Resources/DWARF/$framework"
done
for component in server client; do
    name=kDrive
    [ "$component" != client ] || name=kDrive_client
    for suffix in src.zip 1.src.zip; do
        require_file "$artifacts_dir/sentry-$component-artifacts/$name.$suffix"
        unzip -tq "$artifacts_dir/sentry-$component-artifacts/$name.$suffix"
    done
done
for framework in kDriveCore kDriveCoreUI kDriveResources; do
    for suffix in src.zip 1.src.zip; do
        require_file "$artifacts_dir/sentry-client-artifacts/client-frameworks/$framework.$suffix"
        unzip -tq "$artifacts_dir/sentry-client-artifacts/client-frameworks/$framework.$suffix"
    done
done

lipo -verify_arch x86_64 arm64 "$updater/Contents/MacOS/kDriveRecoveryUpdater"
for bundle in "$client" "$app" "$updater"; do
    codesign --verify --deep --strict --verbose=2 "$bundle"
    codesign -dv "$bundle" 2>&1 | /usr/bin/grep -F 'TeamIdentifier=864VDCS2QY'
done
for target in "$app" "$updater" "$install_dir/kDrive-$version.pkg"; do
    xcrun stapler validate "$target"
done
pkgutil --check-signature "$install_dir/kDrive-$version.pkg"
spctl --assess --type install --verbose=2 "$install_dir/kDrive-$version.pkg"

# Inspect the actual installer payload, not just the staging app.
verify_tmp="$(mktemp -d)"
trap 'rm -rf "$verify_tmp"' EXIT
pkgutil --expand-full "$install_dir/kDrive-$version.pkg" "$verify_tmp/package"
packaged_app="$(find "$verify_tmp/package" -type d -name kDrive.app -prune -print)"
if [ -z "$packaged_app" ] || [ ! -d "$packaged_app" ]; then
    echo "Error: expected exactly one kDrive.app in the installer payload" >&2
    exit 1
fi
codesign --verify --deep --strict --verbose=2 "$packaged_app"
for executable in Contents/MacOS/kDrive Contents/MacOS/kDrive.app/Contents/MacOS/kDrive.gui; do
    cmp "$app/$executable" "$packaged_app/$executable"
done
unzip -tq "$install_dir/sparkle/kDrive-$version.zip"
unzip -q "$install_dir/sparkle/kDrive-$version.zip" -d "$verify_tmp/sparkle"
codesign --verify --deep --strict --verbose=2 "$verify_tmp/sparkle/kDrive.app"
unzip -tq "$install_dir/kDriveRecoveryUpdater-$version.zip"
unzip -q "$install_dir/kDriveRecoveryUpdater-$version.zip" -d "$verify_tmp/updater"
codesign --verify --deep --strict --verbose=2 "$verify_tmp/updater/kDriveRecoveryUpdater.app"

python3 - "$version" "$app" "$client" "$updater" "$install_dir/sparkle/update-macos-$version.xml" <<'PY'
import plistlib
import sys
import xml.etree.ElementTree as ET

version, app, client, updater, feed = sys.argv[1:]
short_version, build = version.rsplit('.', 1)
for bundle in (app, client, updater):
    with open(f'{bundle}/Contents/Info.plist', 'rb') as file:
        info = plistlib.load(file)
    expected_build = build if bundle == client else version
    expected_short = short_version if bundle == client else version
    if str(info['CFBundleVersion']) != expected_build or info['CFBundleShortVersionString'] != expected_short:
        raise SystemExit(f'Version mismatch in {bundle}: expected {version}, got {info}')

sparkle = '{http://www.andymatuschak.org/xml-namespaces/sparkle}'
item = ET.parse(feed).find('./channel/item')
enclosure = item.find('enclosure')
if enclosure.get(f'{sparkle}version') != version or not enclosure.get(f'{sparkle}edSignature'):
    raise SystemExit('Sparkle appcast version or EdDSA signature is missing/incorrect')
if not enclosure.get('url', '').endswith(f'/kDrive-{version}.zip'):
    raise SystemExit('Sparkle archive URL does not match the release version')
if not enclosure.get('downloadUrl', '').endswith(f'/kDrive-{version}.pkg'):
    raise SystemExit('Sparkle installer URL does not match the release version')
PY

echo "Validated macOS redesigned Swift client $version: universal binaries, symbols, signatures, notarization and update artifacts."
