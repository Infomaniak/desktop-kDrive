#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
install_dir="${INSTALL_DIR:-$repo_root/build-macos/client/install}"
artifacts_dir="${ARTIFACTS_DIR:-$repo_root/build-macos}"

bundle_symbols() {
    local dsym="$1" name="$2" destination="$3"
    local dwarf binary archive
    mkdir -p "$destination"
    cp -R "$dsym" "$destination/$name.dSYM"
    for dwarf in "$dsym"/Contents/Resources/DWARF/*; do
        binary="$(basename "$dwarf")"
        # Pass the DWARF file explicitly: kDrive_client.dSYM contains kDrive.gui,
        # so sentry-cli cannot infer its path from the dSYM directory name.
        sentry-cli debug-files bundle-sources --output "$destination" "$dwarf"
        # Universal DWARF files produce one source bundle per architecture.
        for archive in "$destination/${binary%.*}"*.src.zip; do
            [ -s "$archive" ] || continue
            if [ "$binary" != "$name" ]; then
                mv "$archive" "$destination/$name${archive#"$destination/${binary%.*}"}"
            fi
        done
    done
    test -s "$destination/$name.src.zip"
    test -s "$destination/$name.1.src.zip"
}

rm -rf "$artifacts_dir/sentry-server-artifacts" "$artifacts_dir/sentry-client-artifacts"
bundle_symbols "$install_dir/kDrive.dSYM" kDrive "$artifacts_dir/sentry-server-artifacts"
bundle_symbols "$install_dir/kDrive_client.dSYM" kDrive_client "$artifacts_dir/sentry-client-artifacts"
for framework in kDriveCore kDriveCoreUI kDriveResources; do
    bundle_symbols "$install_dir/client-frameworks/$framework.framework.dSYM" "$framework" \
        "$artifacts_dir/sentry-client-artifacts/client-frameworks"
done
