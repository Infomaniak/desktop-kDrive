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

script_directory_path="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
source "$script_directory_path/../build-utils.sh"

program_name="$(basename "$0")"

function display_help {
  echo "$program_name [-h] [-d architecture]"
  echo "  Build the Linux release AppImage for desktop-kDrive."
  echo "where:"
  echo "-h  Show this help text."
}

function get_arch() {
    architecture=$1

    if [[ "$architecture" == "amd64" ]]; then
        echo "x86_64"
    elif [[  "$architecture" == "arm64" ]]; then
        echo "aarch64"
    else 
        echo "Invalid architecture argument: '$architecture'"
        exit 1;
    fi
}

function build_client_via_cmake() {
  build_type=$1
  conan_dependencies_folder=$2

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
  bash /src/infomaniak-build-tools/conan/build_dependencies.sh $build_type --output-dir="$conan_folder"

  conan_toolchain_file="$(find "$conan_folder" -name 'conan_toolchain.cmake' -print -quit 2>/dev/null | head -n 1)"

  if [ ! -f "$conan_toolchain_file" ]; then
    echo "Conan toolchain file not found: $conan_toolchain_file"
    exit 1
  fi

  cd "$build_folder"

  architecture=$(get_host_arch)
  qt_neon_activation="ON"

  if [[ "$architecture" == "amd64" ]]; then
    qt_neon_activation="OFF"
  fi

  cmake -DCMAKE_PREFIX_PATH="$QT_BASE_DIR" \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DQT_FEATURE_neon="$qt_neon_activation" \
      -DCMAKE_BUILD_TYPE=$build_type \
      -DKDRIVE_THEME_DIR="/src/infomaniak" \
      -DBUILD_UNIT_TESTS=0 \
      -DCONAN_DEP_DIR="$conan_dependencies_folder" \
      -DCMAKE_TOOLCHAIN_FILE="$conan_toolchain_file" \
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
}

function move_dependencies() {
  arch="$(get_arch $1)"
  conan_dependencies_folder=$2

  cd /app

  mkdir -p ./usr/plugins
  cp -P -r /opt/qt6.2.3/plugins/* ./usr/plugins/

  cp -P -r /opt/qt6.2.3/libexec ./usr
  cp -P -r /opt/qt6.2.3/resources ./usr
  cp -P -r /opt/qt6.2.3/translations ./usr

  mv "./usr/lib/$arch-linux-gnu/"* ./usr/lib/ || echo "The folder /app/usr/lib/$arch-linux-gnu/ might not exist." >&2

  cp -P /usr/local/lib/libssl.so* ./usr/lib/
  cp -P /usr/local/lib/libcrypto.so* ./usr/lib/

  cp -P -r "/usr/lib/$arch-linux-gnu/nss" ./usr/lib/

  cp -P /opt/qt6.2.3/lib/libQt6WaylandClient.so* ./usr/lib
  cp -P /opt/qt6.2.3/lib/libQt6WaylandEglClientHwIntegration.so* ./usr/lib

  cp -P "$conan_dependencies_folder"/* ./usr/lib

  mkdir -p ./usr/qml

  rm -rf "./usr/lib/$arch-linux-gnu/"
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
}

function build_app_image() {
  architecture=$1

  export LD_LIBRARY_PATH="/app/usr/lib/:/usr/local/lib:/usr/local/lib64:$LD_LIBRARY_PATH"
  /deploy/linuxdeploy/build/bin/linuxdeploy --appdir /app -e /app/usr/bin/kDrive -i /app/kdrive-win.png -d /app/usr/share/applications/kDrive_client.desktop --plugin qt --output appimage -v0
  mv kDrive*.AppImage "/install/kDrive-$architecture.AppImage"
}


function setup_build() {
  mkdir -p /app
  mkdir -p /build

  # Set Qt-6.2
  export QT_BASE_DIR=/opt/qt6.2.3
  export QTDIR="$QT_BASE_DIR"
  export QMAKE="$QT_BASE_DIR/bin/qmake"
  export PATH="$QT_BASE_DIR/bin:$QT_BASE_DIR/libexec:$PATH"
  export LD_LIBRARY_PATH="$QT_BASE_DIR/lib:$LD_LIBRARY_PATH"
  export PKG_CONFIG_PATH="$QT_BASE_DIR/lib/pkgconfig:$PKG_CONFIG_PATH"
}

while :
do
    case "$1" in
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

ulimit -n 4000000

setup_build

if [ ! "$?" -eq "0" ]; then
    echo
    echo "Build setup failed."
    exit 1
fi

echo

architecture="$(get_host_arch)"
build_type="RelWithDebInfo"
conan_dependencies_folder="/build/conan/dependencies"

echo "Building desktop-kDrive application with type ${build_type} via CMake for architecture ${architecture} ..."
build_client_via_cmake "$build_type" "$conan_dependencies_folder"

if [ ! "$?" -eq "0" ]; then
    echo
    echo "CMake build failed."
    exit 1
fi

echo

echo "Moving dependencies ..."
move_dependencies "$architecture" "$conan_dependencies_folder"

if [ ! "$?" -eq "0" ]; then
    echo
    echo "Move of dependencies failed."
    exit 1
fi

echo "Building AppImage ..."
build_app_image "$architecture"

if [ ! "$?" -eq "0" ]; then
    echo
    echo "Build of the AppImage failed."
    exit 1
fi

echo "Build of AppImage successfully completed for architecture ${architecture}."

