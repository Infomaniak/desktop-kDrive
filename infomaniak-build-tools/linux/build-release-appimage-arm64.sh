#! /bin/bash

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

ulimit -n 4000000

set -xe

mkdir -p /app
mkdir -p /build

# Build client
cd /build
mkdir -p client
cd client

CMAKE_PARAMS=()

if [ -n "$APPLICATION_SERVER_URL" ]; then
	CMAKE_PARAMS+=(-DAPPLICATION_SERVER_URL="$APPLICATION_SERVER_URL")
fi

if [ -n "$KDRIVE_VERSION_BUILD" ]; then
	CMAKE_PARAMS+=(-DKDRIVE_VERSION_BUILD="$KDRIVE_VERSION_BUILD")
fi

export KDRIVE_DEBUG=0

build_folder=$PWD
cd /src

conan_folder=/build/conan
conan_dependencies_folder=$conan_folder/dependencies/

build_type="RelWithDebInfo"

bash /src/infomaniak-build-tools/conan/build_dependencies.sh $build_type --output-dir="$conan_folder"

conan_toolchain_file="$(find "$conan_folder" -name 'conan_toolchain.cmake' -print -quit 2>/dev/null | head -n 1)"

if [ ! -f "$conan_toolchain_file" ]; then
  echo "Conan toolchain file not found: $conan_toolchain_file"
  exit 1
fi

source "$(dirname "$conan_toolchain_file")/conanbuild.sh"

source /src/infomaniak-build-tools/conan/common-utils.sh

# Set Qt
QTDIR=$(find_qt_conan_path "/build")
export QTDIR
export QMAKE="$QTDIR/bin/qmake"
export PATH="$QTDIR/bin:$QTDIR/libexec:$PATH"
export LD_LIBRARY_PATH="$QTDIR/lib:$LD_LIBRARY_PATH"
export PKG_CONFIG_PATH="$QTDIR/lib/pkgconfig:$PKG_CONFIG_PATH"

cd "$build_folder"

cmake -DCMAKE_INSTALL_PREFIX=/usr \
    -DQT_FEATURE_neon=ON \
    -DCMAKE_BUILD_TYPE=$build_type \
    -DKDRIVE_THEME_DIR="/src/infomaniak" \
    -DBUILD_UNIT_TESTS=0 \
    -DCMAKE_TOOLCHAIN_FILE="$conan_toolchain_file" \
    -DCONAN_DEP_DIR="$conan_dependencies_folder" \
    "${CMAKE_PARAMS[@]}" \
    /src
make "-j$(nproc)"

objcopy --only-keep-debug ./bin/kDrive ../kDrive.dbg
objcopy --strip-debug ./bin/kDrive
objcopy --add-gnu-debuglink=../kDrive.dbg ./bin/kDrive

objcopy --only-keep-debug ./bin/kDrive_client ../kDrive_client.dbg
objcopy --strip-debug ./bin/kDrive_client
objcopy --add-gnu-debuglink=../kDrive_client.dbg ./bin/kDrive_client

make DESTDIR=/app install

# Move stuff around
cd /app

mkdir -p ./usr/plugins
cp -P -r "$QTDIR"/plugins/* ./usr/plugins/

cp -P -r "$QTDIR"/libexec ./usr
cp -P -r "$QTDIR"/resources ./usr
cp -P -r "$QTDIR"/translations ./usr

mv ./usr/lib/aarch64-linux-gnu/* ./usr/lib/ || echo "The folder /app/usr/lib/aarch64-linux-gnu/ might not exist." >&2

cp -P -r /usr/lib/aarch64-linux-gnu/nss ./usr/lib/

cp -P "$QTDIR"/lib/libQt6WaylandClient.so* ./usr/lib
cp -P "$QTDIR"/lib/libQt6WaylandEglClientHwIntegration.so* ./usr/lib

cp -P $conan_dependencies_folder/* ./usr/lib

rm -rf ./usr/lib/aarch64-linux-gnu/
rm -rf ./usr/lib/kDrive
rm -rf ./usr/lib/cmake
rm -rf ./usr/include
rm -rf ./usr/mkspecs

# Don't bundle kDrivecmd as we don't run it anyway
rm -rf ./usr/bin/kDrivecmd

# Move sync exclude to right location
rm -rf ./etc

cp ./usr/share/icons/hicolor/512x512/apps/kdrive-win.png . # Workaround for linuxeployqt bug, FIXME

# Build AppImage
export LD_LIBRARY_PATH="/app/usr/lib/:$LD_LIBRARY_PATH:/usr/local/lib:/usr/local/lib64:"

/deploy/linuxdeploy/build/bin/linuxdeploy --appdir /app -e /app/usr/bin/kDrive -i /app/kdrive-win.png -d /app/usr/share/applications/kDrive_client.desktop --plugin qt --output appimage -v0

mv kDrive*.AppImage /install/kDrive-arm64.AppImage

