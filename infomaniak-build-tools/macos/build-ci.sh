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
export SIGN_IDENTITY="Developer ID Application: Infomaniak Network SA (864VDCS2QY)"

# Uncomment to build for testing
# export KDRIVE_DEBUG=1

export MACOSX_DEPLOYMENT_TARGET="10.15"
export CODE_SIGN_INJECT_BASE_ENTITLEMENTS="NO"
src_dir="${1-$PWD}"
app_name="kDrive"


# Set Infomaniak Theme
kdrive_dir="$src_dir/infomaniak"

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
	CMAKE_PARAMS+=(-DSOCKETAPI_TEAM_IDENTIFIER_PREFIX="$TEAM_IDENTIFIER.")
fi

build_type="Release"

bash infomaniak-build-tools/conan/build_dependencies.sh "$build_type" "--output-dir=$conan_folder"

conan_toolchain_file="$(find "$conan_folder" -name 'conan_toolchain.cmake' -print -quit 2>/dev/null | head -n 1)"
if [ ! -f "$conan_toolchain_file" ]; then
  echo "Conan toolchain file not found: $conan_toolchain_file"
  exit 1
fi
conan_build_folder="$(dirname "$conan_toolchain_file")"

source "./infomaniak-build-tools/conan/common-utils.sh"
QTDIR="$(find_qt_conan_path "$conan_build_folder")"
export QTDIR
export PATH=/usr/local/bin:"$QTDIR/bin:$PATH"

source "$conan_build_folder/conanrun.sh" # Load conan build script
# Configure
pushd "$build_dir"

# Configure infomaniakdrive
cmake \
	-DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET" \
	-DCMAKE_INSTALL_PREFIX="$install_dir" \
	-DCMAKE_BUILD_TYPE="$build_type" \
	-DKDRIVE_THEME_DIR="$kdrive_dir" \
	-DBUILD_UNIT_TESTS=1 \
	-DCMAKE_TOOLCHAIN_FILE="$conan_toolchain_file" \
	"${CMAKE_PARAMS[@]}" \
	"$src_dir"

# Build kDrive sources
make -j6 all install

popd
