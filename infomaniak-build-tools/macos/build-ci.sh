#!/usr/bin/env bash
#
# Infomaniak kDrive - Desktop
# Copyright (C) 2023-2024 Infomaniak Network SA
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
export QTDIR="$HOME/Qt/6.2.3/macos"

# Uncomment to build for testing
# export KDRIVE_DEBUG=1

export MACOSX_DEPLOYMENT_TARGET="10.15"
export CODE_SIGN_INJECT_BASE_ENTITLEMENTS="NO"
SRCDIR="${1-$PWD}"
APPNAME="kDrive"

# define Qt6 directory
QTDIR="${QTDIR-$HOME/Qt/6.2.3/macos}"
export PATH=$QTDIR/bin:$PATH

# Set Infomaniak Theme
KDRIVE_DIR="$SRCDIR/infomaniak"

# Set build dir
BUILDDIR="$PWD/build-macos/client"

# Set install dir
INSTALLDIR="$PWD/build-macos/client/install"

# Create install dir if needed
mkdir -p build-macos/client
mkdir -p build-macos/client/install

# Backup the existing .app if there is one
if [ -d "$INSTALLDIR/$APPNAME-old.app" ]; then
	rm -rf "$INSTALLDIR/$APPNAME-old.app"
fi

if [ -d "$INSTALLDIR/$APPNAME.app" ]; then
	cp -a "$INSTALLDIR/$APPNAME.app" "$INSTALLDIR/$APPNAME-old.app"
fi

# Prepare additional cmake arguments
if [ -z "$KDRIVE_VERSION_BUILD" ]; then
	KDRIVE_VERSION_BUILD="$(date +%Y%m%d)"
fi

CMAKE_PARAMS=(-DKDRIVE_VERSION_BUILD="$KDRIVE_VERSION_BUILD")

if [ -n "$TEAM_IDENTIFIER" -a -n "$SIGN_IDENTITY" ]; then
	CMAKE_PARAMS+=(-DSOCKETAPI_TEAM_IDENTIFIER_PREFIX="$TEAM_IDENTIFIER.")
fi

# Configure
pushd "$BUILDDIR"

# Configure infomaniakdrive
cmake \
	-DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET" \
	-DCMAKE_INSTALL_PREFIX="$INSTALLDIR" \
	-DCMAKE_BUILD_TYPE=Release \
	-DKDRIVE_THEME_DIR="$KDRIVE_DIR" \
	-DBUILD_UNIT_TESTS=1 \
	"${CMAKE_PARAMS[@]}" \
	"$SRCDIR"

# Build kDrive sources
make -j6 all install

popd
