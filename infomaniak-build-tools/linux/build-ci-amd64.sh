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

program_name="$(basename "$0")"

function display_help {
  echo "$program_name [-h] [-t build-type] [-u unit-tests]"
  echo "  Build the desktop-kDrive application for Linux Amd64 with the specified build type ."
  echo "where:"
  echo "-h  Show this help text."
  echo "-t <build-type>" 
  echo "  Set the type of the build. Defaults to 'debug'. The valid values are: 'debug' or 'release'."
  echo "-u <unit-tests>" 
  echo "  Activate the build of unit tests. Without this flag, unit tests will not be built."
}


unit_tests=0
build_type="debug"

while :
do
    case "$1" in
      -t | --build-type)
          build_type="$2"   
          shift 2
          ;;
      -u | --unit-tests)
          unit_tests=1   
          shift 1
          ;;
      -h | --help)
          display_help 
          exit 0
          ;;
      --) # End of all options
          shift
          break
          ;;
      -*)
          echo "Error: Unknown option: $1" >&2
          exit 1 
          ;;
      *)  # No more options
          break
          ;;
    esac
done


if [[ $build_type == "release" ]]; then
    build_type="Release"
elif [[ $build_type == "debug" ]]; then
    build_type="Debug"
fi

echo "Build type: $build_type"
echo "Unit tests build flag: $unit_tests"


export QT_BASE_DIR="~/Qt/6.2.3"
export QTDIR="$QT_BASE_DIR/gcc_64"
export BASEPATH=$PWD
export CONTENTDIR="$BASEPATH/build-linux"
export build_dir="$CONTENTDIR/build"
export APPDIR="$CONTENTDIR/app"

extract_debug () {
    objcopy --only-keep-debug "$1/$2" $CONTENTDIR/$2-amd64.dbg
    objcopy --strip-debug "$1/$2"
    objcopy --add-gnu-debuglink=$CONTENTDIR/kDrive-amd64.dbg "$1/$2"
}

mkdir -p $APPDIR
mkdir -p $build_dir

export QMAKE=$QTDIR/bin/qmake
export PATH=$QTDIR/bin:$QTDIR/libexec:$PATH
export LD_LIBRARY_PATH=$QTDIR/lib:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=$QTDIR/lib/pkgconfig:$PKG_CONFIG_PATH

# Set defaults
export SUFFIX=""

# Build client
cd $build_dir
mkdir -p $build_dir/client

CMAKE_PARAMS=()

export KDRIVE_DEBUG=0

cmake -B$build_dir -H$BASEPATH \
    -DOPENSSL_ROOT_DIR=/usr/local \
    -DOPENSSL_INCLUDE_DIR=/usr/local/include \
    -DOPENSSL_CRYPTO_LIBRARY=/usr/local/lib64/libcrypto.so \
    -DOPENSSL_SSL_LIBRARY=/usr/local/lib64/libssl.so \
    -DQT_FEATURE_neon=OFF \
    -DCMAKE_BUILDTYPE=$BUILDTYPE \
    -DCMAKE_PREFIX_PATH=$BASEPATH \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBIN_INSTALL_DIR=$build_dir/client \
    -DKDRIVE_VERSION_SUFFIX=$SUFFIX \
    -DKDRIVE_THEME_DIR="$BASEPATH/infomaniak" \
    -DKDRIVE_VERSION_BUILD="$(date +%Y%m%d)" \
    -DBUILD_UNIT_TESTS=$unit_tests \
    "${CMAKE_PARAMS[@]}" \

make -j$(nproc)

extract_debug ./bin kDrive
extract_debug ./bin kDrive_client

make DESTDIR=$APPDIR install

cp $BASEPATH/sync-exclude-linux.lst $build_dir/bin/sync-exclude.lst
