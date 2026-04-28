#! /bin/bash

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

script_directory_path="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
source "$script_directory_path/build-utils.sh"

program_name="$(basename "$0")"

QT_BASE_DIR=""

function display_help {
  echo "$program_name [-h]" >&2
  echo "  Build the Linux release AppImage for desktop-kDrive." >&2
  echo "where:" >&2
  echo "-h  Show this help text." >&2
}

function get_arch() {
    architecture=$1

    if [[ "$architecture" == "amd64" ]]; then
        echo "x86_64"
    elif [[  "$architecture" == "arm64" ]]; then
        echo "aarch64"
    else
        echo "Invalid architecture argument: '$architecture'" >&2
        exit 1;
    fi
}

function find_qt_from_conan() {
  build_type=$1

  echo "Building Conan dependencies and detecting Qt installation..."

  # Ensure /build directory exists
  mkdir -p /build/client
  cd /build/client

  build_folder=$PWD
  cd /src

  # Build Conan dependencies
  conan_folder=/build/conan
  bash /src/infomaniak-build-tools/conan/build_dependencies.sh "$build_type" --output-dir="$conan_folder" --clean-cache

  if [ ! "$?" -eq "0" ]; then
    echo "ERROR: Conan dependencies build failed" >&2
    exit 1
  fi

  # Source utilities for Qt detection
  source /src/infomaniak-build-tools/conan/common-utils.sh

  # Detect Qt installation from Conan
  QT_BASE_DIR="$(find_qt_conan_path "$conan_folder")"

  if [ -z "$QT_BASE_DIR" ] || [ ! -d "$QT_BASE_DIR" ]; then
    echo "ERROR: Qt base directory not found via Conan (QT_BASE_DIR='$QT_BASE_DIR')" >&2
    exit 1
  fi
}

function build_client_via_cmake() {
  build_type=$1
  conan_dependencies_folder=$2

  echo "Building client via CMake..."

  cd /build/client

  CMAKE_PARAMS=()

  if [ -n "$APPLICATION_SERVER_URL" ]; then
  	CMAKE_PARAMS+=(-DAPPLICATION_SERVER_URL="$APPLICATION_SERVER_URL")
  fi

  export KDRIVE_DEBUG=0

  # Find Conan toolchain file
  conan_folder=/build/conan
  conan_toolchain_file="$(find "$conan_folder" -name 'conan_toolchain.cmake' -print -quit 2>/dev/null | head -n 1)"

  if [ ! -f "$conan_toolchain_file" ]; then
    echo "ERROR: Conan toolchain file not found: $conan_toolchain_file" >&2
    exit 1
  fi

  architecture=$(get_host_arch)
  qt_neon_activation="ON"

  if [[ "$architecture" == "amd64" ]]; then
    qt_neon_activation="OFF"
  fi

  cmake -DCMAKE_PREFIX_PATH="$QT_BASE_DIR" \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DQT_FEATURE_neon="$qt_neon_activation" \
      -DCMAKE_MODULE_PATH="$QT_BASE_DIR/lib/cmake/" \
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

  bundle_sources_for_sentry ../kDrive.dbg
  bundle_sources_for_sentry ../kDrive_client.dbg

  make DESTDIR=/app install
}

function bundle_sources_for_sentry() {
  local debug_file="$1"
  local source_bundle="${debug_file%.dbg}.src.zip"

  if ! command -v sentry-cli >/dev/null 2>&1; then
    echo "WARNING: sentry-cli not found, skipping source bundle generation for '${debug_file}'." >&2
    return
  fi

  echo "Bundling sources for $(basename "$debug_file")..."
  rm -f "$source_bundle"
  sentry-cli debug-files bundle-sources "$debug_file"

  if [ ! -f "$source_bundle" ]; then
    echo "ERROR: Source bundle '${source_bundle}' was not created for '${debug_file}'." >&2
    exit 1
  fi
}

function move_dependencies() {
  arch="$(get_arch $1)"
  conan_dependencies_folder=$2

  cd /app

  mkdir -p ./usr/plugins
  cp -P -r $QT_BASE_DIR/plugins/* ./usr/plugins/

  cp -P -r $QT_BASE_DIR/libexec ./usr
  cp -P -r $QT_BASE_DIR/resources ./usr || true # resources might not exist, ignore if not found
  cp -P -r $QT_BASE_DIR/translations ./usr

  mv "./usr/lib/$arch-linux-gnu/"* ./usr/lib/ 2>/dev/null || echo "The folder /app/usr/lib/$arch-linux-gnu/ might not exist." >&2

  cp -P "$conan_dependencies_folder"/* ./usr/lib

  # Copy nss if it exists (optional)
  if [ -d "/usr/lib/$arch-linux-gnu/nss" ]; then
    cp -P -r "/usr/lib/$arch-linux-gnu/nss" ./usr/lib/ 2>/dev/null
  fi

  # Copy Qt Wayland libraries if they exist (optional for Wayland support)
  for lib in libQt6WaylandClient.so libQt6WaylandEglClientHwIntegration.so; do
    if ls $QT_BASE_DIR/lib/${lib}* 1> /dev/null 2>&1; then
      cp -P $QT_BASE_DIR/lib/${lib}* ./usr/lib 2>/dev/null || true
    fi
  done

  mkdir -p ./usr/qml

  # Copy GIO modules from the build system
  # These are loaded dynamically by GLib and must match the bundled GLib version
  mkdir -p ./usr/lib/gio/modules
  if [ -d "/usr/lib/$arch-linux-gnu/gio/modules" ]; then
    echo "Copying GIO modules from /usr/lib/$arch-linux-gnu/gio/modules..."
    cp -P /usr/lib/$arch-linux-gnu/gio/modules/*.so ./usr/lib/gio/modules/ 2>/dev/null || true
  fi

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

function clean_app_directory() {
  arch="$(get_arch $1)"

  echo "Cleaning unnecessary files from AppImage..."

  cd /app

  echo "  Removing Qt development tools..."
  cd ./usr/libexec
  find . -mindepth 1 -maxdepth 1 \( -type f -o -type l \) -delete
  cd /app

  # Remove development plugins
  echo "  Removing development plugins..."
  rm -rf ./usr/plugins/designer
  rm -rf ./usr/plugins/qmltooling
  rm -rf ./usr/plugins/assetimporters
  rm -rf ./usr/plugins/canbus
  rm -rf ./usr/plugins/geometryloaders
  rm -rf ./usr/plugins/opcua
  rm -rf ./usr/plugins/renderers
  rm -rf ./usr/plugins/renderplugins
  rm -rf ./usr/plugins/sceneparsers
  rm -rf ./usr/plugins/scxmldatamodel
  rm -rf ./usr/plugins/sensors
  rm -rf ./usr/plugins/virtualkeyboard
  rm -rf ./usr/plugins/wayland-graphics-integration-server

  # Remove unnecessary platform plugins (keep only xcb and wayland)
  echo "  Removing unnecessary platform plugins..."
  cd ./usr/plugins/platforms
  find . -mindepth 1 -maxdepth 1 \( -type f -o -type l \) ! -name "libqxcb.so" ! -name "libqwayland*" -delete
  cd /app

  # Remove egldeviceintegrations (embedded systems)
  rm -rf ./usr/plugins/egldeviceintegrations

  # Remove unnecessary SQL drivers (keep only sqlite)
  echo "  Removing unnecessary SQL drivers..."
  cd ./usr/plugins/sqldrivers
  find . -mindepth 1 -maxdepth 1 \( -type f -o -type l \) ! -name "libqsqlite.so" -delete
  cd /app

  # Clean translations - keep only de, es, fr, it, sv, pt, pl, nb, fi, da, en
  echo "  Cleaning translations..."
  cd ./usr/translations

  # Remove all assistant, designer, linguist, qt_help translations (dev tools)
  rm -f assistant_*.qm designer_*.qm linguist_*.qm qt_help_*.qm

  # For other Qt translations, keep only supported languages
  for prefix in qt qtbase qtconnectivity qtdeclarative qtlocation qtmultimedia qtserialport qtwebsockets; do
    # Remove all except de, es, fr, it, sv, pt, pl, nb, fi, da, el, en
    find . -mindepth 1 -maxdepth 1 \( -type f -o -type l \) -name "${prefix}_*.qm" \
      ! -name "*_de.qm" ! -name "*_es.qm" ! -name "*_fr.qm" ! -name "*_it.qm" ! -name "*_sv.qm" ! -name "*_pt.qm" ! -name "*_pl.qm" ! -name "*_nb.qm" ! -name "*_fi.qm" ! -name "*_da.qm" ! -name "*_el.qm" ! -name "*_en.qm" -delete
  done

  cd /app

  # Remove unnecessary system libraries
  echo "  Removing unnecessary system libraries..."
  cd ./usr/lib

  # Remove Kerberos/GSSAPI libs
  rm -f libgssapi*.so.* libk5crypto.so.* libkeyutils.so.* libkrb5.so.* libkrb5support.so.*
  rm -f libhcrypto.so.* libheimbase.so.* libheimntlm.so.* libhx509.so.* libroken.so.* libwind.so.* libasn1.so.*

  # Remove LDAP libs
  rm -f liblber*.so.* libldap*.so.* libsasl2.so.*

  # Remove Avahi
  rm -f libavahi-client.so.* libavahi-common.so.*

  # Remove CUPS (printing) - uncomment if printing not needed
  # rm -f libcups.so.*

  # Remove old OpenSSL 1.1 (we have 3.x)
  rm -f libcrypto.so.1.1 libssl.so.1.1

  # Remove unused image format libs
  rm -f libjbig.so.* librtmp.so.* libssh.so.*

  cd /app

  echo "  Cleanup completed!"
}

function build_app_image() {
  architecture=$1

  export LD_LIBRARY_PATH="/app/usr/lib/:/usr/local/lib:/usr/local/lib64:$LD_LIBRARY_PATH"
  # linuxdeploy bundles its own `strip` which can fail on newer ELF features.
  # Skipping linuxdeploy stripping avoids build failures; our binaries are already handled earlier.
  export NO_STRIP=1
  linuxdeploy --appdir /app -e /app/usr/bin/kDrive -i /app/kdrive-win.png -d /app/usr/share/applications/kDrive_client.desktop --plugin qt --output appimage -v0 --
  mv kDrive*.AppImage "/install/kDrive-$architecture.AppImage"
}


function setup_build() {
  # Validate that QT_BASE_DIR is set
  if [ -z "$QT_BASE_DIR" ]; then
    echo "ERROR: QT_BASE_DIR is not set. Call find_qt_from_conan() first." >&2
    exit 1
  fi

  if [ ! -d "$QT_BASE_DIR" ]; then
    echo "ERROR: QT_BASE_DIR directory does not exist: $QT_BASE_DIR" >&2
    exit 1
  fi

  echo "Setting up build environment with Qt from: $QT_BASE_DIR"

  mkdir -p /app
  mkdir -p /build

  # Export Qt environment variables
  export QT_BASE_DIR
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

inode_max_limit=100000
ulimit_error=$( { ulimit -n $inode_max_limit; } 2>&1)
if [[ -n "$ulimit_error" ]]; then
    echo "Failed to set the max limit of open inodes with '$inode_max_limit'." >&2
    echo "Current limit: '$(ulimit -n)'." >&2
fi

architecture="$(get_host_arch)"
build_type="RelWithDebInfo"
conan_dependencies_folder="/build/conan/dependencies"

echo "Detecting Qt and building Conan dependencies for ${architecture}..."
find_qt_from_conan "$build_type"

if [ ! "$?" -eq "0" ]; then
    printf "\nQt detection or Conan dependencies build failed." >&2
    exit 1
fi

echo

setup_build

if [ ! "$?" -eq "0" ]; then
    printf "\nBuild setup failed." >&2
    exit 1
fi

echo

echo "Building desktop-kDrive application with type ${build_type} via CMake for architecture ${architecture}..."
build_client_via_cmake "$build_type" "$conan_dependencies_folder"

if [ ! "$?" -eq "0" ]; then
    printf "\nCMake build failed." >&2
    exit 1
fi

echo

echo "Moving dependencies ..."
move_dependencies "$architecture" "$conan_dependencies_folder"

if [ ! "$?" -eq "0" ]; then
    printf "\nMove of dependencies failed." >&2
    exit 1
fi

# TODO enable the cleaning of the appimage folder once the build is stable.
echo
echo "Cleaning unnecessary files ..."
clean_app_directory "$architecture"

echo
echo "Building AppImage ..."
build_app_image "$architecture"

if [ ! "$?" -eq "0" ]; then
    printf "\nBuild of the AppImage failed." >&2
    exit 1
fi

echo "Build of AppImage successfully completed for architecture ${architecture}."
