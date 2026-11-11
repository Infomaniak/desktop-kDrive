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
source "$script_directory_path/appimage.sh"

program_name="$(basename "$0")"

QT_BASE_DIR=""

# Runs an appimage.sh step in a new shell. Called from a `||` list, Bash would otherwise ignore the `set -e`
# of the step and of its subshell, and continue after a failed command.
function run_appimage_step() {
  bash -c 'source "$1" && shift && "$@"' run_appimage_step "$script_directory_path/appimage.sh" "$@"
}

function display_help {
  echo "$program_name [-h]" >&2
  echo "  Build the Linux release AppImage for desktop-kDrive." >&2
  echo "where:" >&2
  echo "-h  Show this help text." >&2
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
  bash /src/infomaniak-build-tools/conan/build_dependencies.sh "$build_type" --output-dir="$conan_folder" --clean-cache --update

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

  conan_folder=/build/conan
  conan_toolchain_file="$(find "$conan_folder" -name 'conan_toolchain.cmake' -print -quit 2>/dev/null | head -n 1)"

  if [ ! -f "$conan_toolchain_file" ]; then
    echo "ERROR: Conan toolchain file not found: $conan_toolchain_file" >&2
    return 1
  fi

  # Ensure cmake is the one provided by Conan: Qt >= 6.12 requires CMake >= 3.25 to be
  # consumed via find_package() (see QtPublicCMakeVersionHelpers.cmake).
  # shellcheck disable=SC1091
  source "$(dirname "$conan_toolchain_file")/conanbuild.sh"

  cmake -DCMAKE_PREFIX_PATH="$QT_BASE_DIR" \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DQT_FEATURE_neon=OFF \
      -DCMAKE_MODULE_PATH="$QT_BASE_DIR/lib/cmake/" \
      -DCMAKE_BUILD_TYPE=$build_type \
      -DKDRIVE_THEME_DIR="/src/infomaniak" \
      -DBUILD_UNIT_TESTS=0 \
      -DBUILD_GUI=ON \
      -DBUILD_GUI_LEGACY=OFF \
      -DKDRIVE_DEPLOY_QT_RUNTIME=ON \
      -DQT_ENABLE_VERBOSE_DEPLOYMENT=ON \
      -DCONAN_DEP_DIR="$conan_dependencies_folder" \
      -DCMAKE_TOOLCHAIN_FILE="$conan_toolchain_file" \
      "${CMAKE_PARAMS[@]}" \
      /src || return 1
  make "-j$(nproc)" || return 1

  run_appimage_step appimage_extract_debug_symbols ./bin /build kDrive kdrive_qml || return 1

  bundle_sources_for_sentry /build/kDrive.dbg
  bundle_sources_for_sentry /build/kdrive_qml.dbg

  make DESTDIR=/app install || return 1
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
  # Repository-relative paths in the debug info are resolved from the current directory.
  (cd /src && sentry-cli debug-files bundle-sources "$debug_file")

  if [ ! -f "$source_bundle" ]; then
    echo "ERROR: Source bundle '${source_bundle}' was not created for '${debug_file}'." >&2
    exit 1
  fi
}

function build_app_image() {
  run_appimage_step appimage_prepare_appdir /app || return 1
  run_appimage_step appimage_check_appdir /app || return 1

  cd /build || return 1
  run_appimage_step appimage_linuxdeploy_deploy /app || return 1
  run_appimage_step appimage_strip_unneeded_symbols /app || return 1
  run_appimage_step appimage_verify_bundle /app || return 1
  run_appimage_step appimage_package /app || return 1

  mv kDrive*.AppImage "/install/kDrive-$architecture.AppImage"
}

function build_recovery_updater_image() {
  architecture=$1
  updater_bin="/app/usr/bin/kDriveRecoveryUpdater"

  if [ ! -f "$updater_bin" ]; then
    echo "kDriveRecoveryUpdater not found at '$updater_bin'; the release requires its AppImage." >&2
    return 1
  fi

  echo "Building recovery updater AppImage for ${architecture}..."

  updater_appdir="/tmp/updater-app"
  updater_icon="/src/infomaniak/theme/colored/512-kdrive-recovery-updater-icon.png"
  run_appimage_step appimage_prepare_recovery_updater_appdir /app "$updater_appdir" "$updater_icon" || return 1
  run_appimage_step appimage_check_recovery_updater_appdir "$updater_appdir" || return 1

  cd /build || return 1
  run_appimage_step appimage_linuxdeploy_recovery_updater "$updater_appdir" || return 1
  run_appimage_step appimage_strip_unneeded_symbols "$updater_appdir" || return 1
  run_appimage_step appimage_verify_bundle "$updater_appdir" || return 1
  run_appimage_step appimage_package "$updater_appdir" || return 1

  mv kDriveRecoveryUpdater*.AppImage "/install/kDriveRecoveryUpdater-$architecture.AppImage"
  if [ "$?" -ne 0 ]; then
    echo "ERROR: Failed to move recovery updater AppImage to /install/" >&2
    return 1
  fi

  echo "Recovery updater AppImage created: /install/kDriveRecoveryUpdater-$architecture.AppImage"
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
check_linux_v4_version /src || exit 1

# The sentry recipe requires libc-ares-dev, which the build image does not provide.
# TODO: Remove once the build image ships libc-ares-dev.
if ! dpkg-query -W -f='${Status}' libc-ares-dev 2>/dev/null | grep -q "install ok installed"; then
    echo "Installing libc-ares-dev..."
    if ! { apt-get update -qq && apt-get install -y -qq libc-ares-dev; }; then
        echo "Unable to install libc-ares-dev." >&2
        exit 1
    fi
fi

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
build_client_via_cmake "$build_type" "$conan_dependencies_folder" || {
    printf "\nCMake build failed." >&2
    exit 1
}

echo
echo "Building recovery updater AppImage ..."
build_recovery_updater_image "$architecture" || {
    printf "\nBuild of the recovery updater AppImage failed." >&2
    exit 1
}

rm -f /app/usr/bin/kDriveRecoveryUpdater
run_appimage_step check_main_appdir_updater_separation /app || {
    printf "\nRecovery updater separation check failed." >&2
    exit 1
}

echo
echo "Building AppImage ..."
build_app_image || {
    printf "\nBuild of the AppImage failed." >&2
    exit 1
}

echo "Build of AppImage successfully completed for architecture ${architecture}."
