#!/usr/bin/env bash

#
# Infomaniak kDrive - Desktop
# Copyright (C) 2023-2026 Infomaniak Network SA
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

#
# Generate the Sparkle auto-update files (signed archive + appcast) for a
# previously built and *signed* kDrive.app.
#
# This mirrors the "Generate Sparkle artifacts" step of the macOS CI action
# (.github/actions/build_macos/action.yml) so a local release build can produce
# the same artifacts. It must run AFTER the app has been signed (the appcast
# EdDSA signature covers the archived .app) and the .pkg has been created (the
# appcast download URL points at it).
#
# The version is read exclusively from version.json (authoritative source).
#
# Environment overrides:
#   INSTALL_DIR  Directory containing kDrive.app and kDrive-<version>.pkg
#                (default: <repo>/build-macos/client/install)
#   SPARKLE_BIN  Directory containing the Sparkle "generate_appcast" tool
#                (default: $HOME/Sparkle/bin)
#
# Outputs (in $INSTALL_DIR/sparkle):
#   kDrive-<version>.zip          Signed Sparkle archive of kDrive.app
#   update-macos-<version>.xml    Sparkle appcast feed
#

set -e
set -o pipefail

# Resolve the repository root from this script's location so it can be run
# from anywhere.
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"

# Read the version from the authoritative version.json.
source "$repo_root/infomaniak-build-tools/version-helpers.sh"
version="$(GetVersionFromJson "$repo_root" true)"

if [ -z "$version" ]; then
	echo "Error: could not read the version from version.json" >&2
	exit 1
fi

app_name="kDrive"
install_dir="${INSTALL_DIR:-$repo_root/build-macos/client/install}"
sparkle_dir="$install_dir/sparkle"
sparkle_bin="${SPARKLE_BIN:-$HOME/Sparkle/bin}"
generate_appcast="$sparkle_bin/generate_appcast"
insert_xml="$repo_root/infomaniak-build-tools/macos/insert_xml.py"
venv_dir="$repo_root/infomaniak-build-tools/macos/venv"

echo "Generating Sparkle files for $app_name-$version"

# --- Preflight checks --------------------------------------------------------
if [ ! -d "$install_dir/$app_name.app" ]; then
	echo "Error: $install_dir/$app_name.app not found." >&2
	echo "Build and sign the app before generating the Sparkle files." >&2
	exit 1
fi

if [ ! -x "$generate_appcast" ]; then
	echo "Error: Sparkle 'generate_appcast' not found at: $generate_appcast" >&2
	echo "Install Sparkle or set SPARKLE_BIN to its bin directory." >&2
	exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
	echo "Error: python3 is required (used by insert_xml.py)." >&2
	exit 1
fi

if [ ! -f "$install_dir/$app_name-$version.pkg" ]; then
	echo "Warning: $install_dir/$app_name-$version.pkg not found." >&2
	echo "The appcast download URL points at this package; make sure it is published." >&2
fi

# --- Clean up the temporary venv on exit ------------------------------------
cleanup() {
	if [ -n "${VIRTUAL_ENV:-}" ]; then
		deactivate || true
	fi
	rm -rf "$venv_dir"
}
trap cleanup EXIT

# --- (Re)create the Sparkle output directory (idempotent) --------------------
rm -rf "$sparkle_dir"
mkdir -p "$sparkle_dir"

# --- 1. Archive the signed .app ---------------------------------------------
echo "  - Archiving $app_name.app"
/usr/bin/ditto -c -k --sequesterRsrc --keepParent \
	"$install_dir/$app_name.app" "$sparkle_dir/$app_name-$version.zip"

# --- 2. Generate and sign the appcast ---------------------------------------
# generate_appcast signs the archive with the EdDSA private key stored in the
# keychain and writes an appcast file named after the SUFeedURL basename
# ("desktopclient").
echo "  - Generating appcast"
"$generate_appcast" "$sparkle_dir"

# --- 3. Rewrite the host and tag the first release notes link as English -----
echo "  - Rewriting appcast host and release notes link"
sed -e 's/www.infomaniak.com\/drive\/update/download.storage.infomaniak.com\/drive\/desktopclient/' \
	-e 's/sparkle:releaseNotesLink/sparkle:releaseNotesLink xml:lang="en"/' \
	"$sparkle_dir/desktopclient" > "$sparkle_dir/update-macos-$version.xml"

# --- 4. Insert localized release notes links and the .pkg download URL -------
echo "  - Inserting localized release notes links and download URL"
python3 -m venv "$venv_dir"
source "$venv_dir/bin/activate"
pip3 install bs4 lxml
python3 "$insert_xml" "$sparkle_dir/update-macos-$version.xml" "$app_name-$version"

echo "Sparkle files generated in $sparkle_dir:"
echo "  - $app_name-$version.zip"
echo "  - update-macos-$version.xml"
