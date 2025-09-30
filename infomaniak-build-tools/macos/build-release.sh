#!/usr/bin/env bash

#
# Infomaniak kDrive - Desktop
# Copyright (C) 2023-2025 Infomaniak Network SA
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

set -e
set -o pipefail
set -x

export TEAM_IDENTIFIER="864VDCS2QY"
export APP_DOMAIN="com.infomaniak.drive.desktopclient"
export SIGN_IDENTITY="Developer ID Application: Infomaniak Network SA (864VDCS2QY)"
export INSTALLER_SIGN_IDENTITY="Developer ID Installer: Infomaniak Network SA (864VDCS2QY)"
export QTDIR="$HOME/Qt/6.2.3/macos"

# Uncomment to build for testing
# export KDRIVE_DEBUG=1

export MACOSX_DEPLOYMENT_TARGET="10.15"
export CODE_SIGN_INJECT_BASE_ENTITLEMENTS="NO"
src_dir="${1-$PWD}"
app_name="kDrive"

# define Qt6 directory
QT_DIR="${QT_DIR-$HOME/Qt/6.2.3/macos}"
export PATH="$QT_DIR/bin:$PATH"

# Set Infomaniak Theme
kdrive_dir="$src_dir/infomaniak"

# Path to Sparkle installation
sparkle_dir="$HOME/Library/Frameworks"

# Set build dir
build_dir="$PWD/build-macos/client"


# Set conan dir
conan_folder="$build_dir/conan"
mkdir -p "$conan_folder"

# Set install dir
install_dir="$PWD/build-macos/client/install"
mkdir -p "$install_dir"

# Backup the existing .app if there is one
if [ -d "$install_dir/$app_name-old.app" ]; then
	rm -rf "$install_dir/$app_name-old.app"
fi

if [ -d "$install_dir/$app_name.app" ]; then
	cp -a "$install_dir/$app_name.app" "$install_dir/$app_name-old.app"
fi

# Prepare additional cmake arguments
if [ -z "$KDRIVE_VERSION_BUILD" ]; then
	KDRIVE_VERSION_BUILD="$(date +%Y%m%d)"
fi

CMAKE_PARAMS=(-DKDRIVE_VERSION_BUILD="$KDRIVE_VERSION_BUILD")

if [ -n "$TEAM_IDENTIFIER" ] && [ -n "$SIGN_IDENTITY" ]; then
	CMAKE_PARAMS+=(-DTEAM_IDENTIFIER_PREFIX="$TEAM_IDENTIFIER.")
fi

bash infomaniak-build-tools/conan/build_dependencies.sh Release --output-dir="$conan_folder" --make-release

conan_toolchain_file="$(find "$conan_folder" -name 'conan_toolchain.cmake' -print -quit 2>/dev/null | head -n 1)"

if [ ! -f "$conan_toolchain_file" ]; then
  echo "Conan toolchain file not found: $conan_toolchain_file"
  exit 1
fi

# Configure
pushd "$build_dir"

cmake \
	-DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET" \
	-DCMAKE_INSTALL_PREFIX="$install_dir" \
	-DCMAKE_BUILD_TYPE=Release \
	-DSPARKLE_LIBRARY="$sparkle_dir/Sparkle.framework" \
	-DKDRIVE_THEME_DIR="$kdrive_dir" \
	-DCMAKE_TOOLCHAIN_FILE="$conan_toolchain_file" \
	"${CMAKE_PARAMS[@]}" \
	"$src_dir"

# Build
make -j6 all install

# Generate Debug Symbol files
dsymutil ./install/kDrive.app/Contents/MacOS/kDrive -o ./install/kDrive.dSYM
dsymutil ./bin/kDrive_client -o ./install/kDrive_client.dSYM

popd

# Sign
sign_files=()

if [ -n "$TEAM_IDENTIFIER" ] && [ -n "$APP_DOMAIN" ] && [ -n "$SIGN_IDENTITY" ]; then
	"$src_dir/admin/osx/sign_app.sh" "$install_dir/$app_name.app" "$SIGN_IDENTITY" "$TEAM_IDENTIFIER" "$APP_DOMAIN"
	sign_files+=("$install_dir/$app_name.app")
fi

if [ -n "$INSTALLER_SIGN_IDENTITY" ]; then
	# xcrun stapler staple $package_file
	"$build_dir/admin/osx/create_mac.sh" "$install_dir" "$build_dir" "$INSTALLER_SIGN_IDENTITY"
	package_file=$(grep "installer=" "$build_dir/admin/osx/create_mac.sh" | sed -E 's/installer="([^"]+)"/\1/')
	sign_files+=("$install_dir/$package_file.pkg")
else
	"$build_dir/admin/osx/create_mac.sh" "$install_dir" "$build_dir"
fi

# Notarise
if [ -n "$sign_files" ]; then
	rm -rf "$install_dir/notorization" "$install_dir/notarization/"
	mkdir -p "$install_dir/notarization"

	for file in "${sign_files[@]}"; do
		cp -a "$file" "$install_dir/notarization"
	done

	# Prepare for notarization
	echo "Preparing for notarization"
	/usr/bin/ditto -c -k --keepParent "$install_dir/notarization" "$install_dir/InfomaniakDrive.zip"
	# Send to notarization
	echo "Sending notarization request"
	xcrun notarytool submit --apple-id "$ALTOOL_USERNAME" --keychain-profile "notarytool" "$install_dir/InfomaniakDrive.zip" --progress --wait
fi
