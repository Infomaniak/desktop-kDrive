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

set -xe

BUILDTYPE="Debug"
if [[ $1 == 'release' ]]; then
    BUILDTYPE="Release";
fi

export QT_BASE_DIR="~/Qt/6.2.3"
export QTDIR="$QT_BASE_DIR/gcc_64"
export BASEPATH=$PWD
export CONTENTDIR="$BASEPATH/build-linux"
export BUILDDIR="$CONTENTDIR/build"
export APPDIR="$CONTENTDIR/app"

extract_debug () {
    objcopy --only-keep-debug "$1/$2" $CONTENTDIR/$2-amd64.dbg
    objcopy --strip-debug "$1/$2"
    objcopy --add-gnu-debuglink=$CONTENTDIR/kDrive-amd64.dbg "$1/$2"
}

mkdir -p $APPDIR
mkdir -p $BUILDDIR

export QMAKE=$QTDIR/bin/qmake
export PATH=$QTDIR/bin:$QTDIR/libexec:$PATH
export LD_LIBRARY_PATH=$QTDIR/lib:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=$QTDIR/lib/pkgconfig:$PKG_CONFIG_PATH

# Set defaults
export SUFFIX=""

# Build client
cd $BUILDDIR
mkdir -p $BUILDDIR/client

CMAKE_PARAMS=()

export KDRIVE_DEBUG=0

# Configure code coverage computation
if [ ! -f "${HOME}/BullseyeCoverageEnv.txt" ]; then
	# Tells BullseyeCoverage where to store the coverage information generated during the build and the run of the tests
	echo "COVFILE=${BASEPATH}/src/test.cov" >  "${HOME}/BullseyeCoverageEnv.txt"
fi


cmake -B$BUILDDIR -H$BASEPATH \
    -DOPENSSL_ROOT_DIR=/usr/local \
    -DOPENSSL_INCLUDE_DIR=/usr/local/include \
    -DOPENSSL_CRYPTO_LIBRARY=/usr/local/lib64/libcrypto.so \
    -DOPENSSL_SSL_LIBRARY=/usr/local/lib64/libssl.so \
    -DQT_FEATURE_neon=OFF \
    -DCMAKE_BUILDTYPE=$BUILDTYPE \
    -DCMAKE_PREFIX_PATH=$BASEPATH \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBIN_INSTALL_DIR=$BUILDDIR/client \
    -DKDRIVE_VERSION_SUFFIX=$SUFFIX \
    -DKDRIVE_THEME_DIR="$BASEPATH/infomaniak" \
    -DKDRIVE_VERSION_BUILD="$(date +%Y%m%d)" \
    -DBUILD_UNIT_TESTS=1 \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
    "${CMAKE_PARAMS[@]}" \

make -j$(nproc)

extract_debug ./bin kDrive
extract_debug ./bin kDrive_client

make DESTDIR=$APPDIR install

cp $BASEPATH/sync-exclude-linux.lst $BUILDDIR/bin/sync-exclude.lst
