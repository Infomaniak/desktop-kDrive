#! /bin/bash

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

ulimit -n 4000000

set -xe

mkdir -p /app
mkdir -p /build

# Set Qt-6.2
export QT_BASE_DIR=/opt/qt6.2.3
export QTDIR=$QT_BASE_DIR
export QMAKE=$QT_BASE_DIR/bin/qmake
export PATH=$QT_BASE_DIR/bin:$QT_BASE_DIR/libexec:$PATH
export LD_LIBRARY_PATH=$QT_BASE_DIR/lib:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=$QT_BASE_DIR/lib/pkgconfig:$PKG_CONFIG_PATH

# Set defaults
export SUFFIX="master"

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

cmake -DCMAKE_PREFIX_PATH=$QT_BASE_DIR \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DQT_FEATURE_neon=OFF \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DKDRIVE_VERSION_SUFFIX=$SUFFIX \
    -DKDRIVE_THEME_DIR="/src/infomaniak" \
    -DWITH_CRASHREPORTER=0 \
    -DBUILD_UNIT_TESTS=0 \
    "${CMAKE_PARAMS[@]}" \
    /src
make -j4

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
cp -P -r /opt/qt6.2.3/plugins/* ./usr/plugins/

cp -P -r /opt/qt6.2.3/libexec ./usr
cp -P -r /opt/qt6.2.3/resources ./usr
cp -P -r /opt/qt6.2.3/translations ./usr

mv ./usr/lib/x86_64-linux-gnu/* ./usr/lib/

mkdir -p ./usr/qml

rm -rf ./usr/lib/x86_64-linux-gnu/
rm -rf ./usr/lib/kDrive
rm -rf ./usr/lib/cmake
rm -rf ./usr/include
rm -rf ./usr/mkspecs

# Don't bundle kDrivecmd as we don't run it anyway
rm -rf ./usr/bin/kDrivecmd

# Move sync exclude to right location
cp /src/sync-exclude-linux.lst ./usr/bin/sync-exclude.lst
rm -rf ./etc

cp ./usr/share/icons/hicolor/512x512/apps/kdrive-win.png . # Workaround for linuxeployqt bug, FIXME

# Because distros need to get their shit together
cp -P /usr/local/lib/libssl.so* ./usr/lib/
cp -P /usr/local/lib/libcrypto.so* ./usr/lib/

# NSS fun
cp -P -r /usr/lib/x86_64-linux-gnu/nss ./usr/lib/

# Build AppImage
export LD_LIBRARY_PATH=/app/usr/lib/:/usr/local/lib:/usr/local/lib64:$LD_LIBRARY_PATH

/deploy/linuxdeploy/build/bin/linuxdeploy --appdir /app -e /app/usr/bin/kDrive -i /app/kdrive-win.png -d /app/usr/share/applications/kDrive_client.desktop --plugin qt --output appimage -v0

mv kDrive*.AppImage /install/kDrive-${SUFFIX}-x86_64.AppImage

