#!/usr/bin/env bash

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

set -e

script_directory_path="$(cd -- "$(dirname "$0")" >/dev/null 2>&1; pwd -P)"
source "$script_directory_path/build-utils.sh"
source "$script_directory_path/appimage-v4.sh"

program_name="$(basename "$0")"


function get_default_src_dir() {
  if [[ -n "$KDRIVE_SRC_DIR" ]]; then
     echo "$KDRIVE_SRC_DIR"
  elif [[ -d "$HOME/Projects/desktop-kDrive" ]]; then
     echo "$HOME/Projects/desktop-kDrive"
  else
     echo "$PWD"
    fi
}
build_unit_tests=0

src_dir="$(get_default_src_dir)"

function display_help {
  echo "$program_name [-h] [-d git-directory] [-u]"
  echo "  Build the Linux Amd64 release executables built inside <git-directory>/build-linux."
  echo "where:"
  echo "-h  Show this help text."
  echo "-d <git-directory>" 
  echo "-u  Activate the build of unit tests. Without this flag, unit tests will not be built."
  echo "  Set the path to the desktop-kDrive git directory. Defaults to '$src_dir'."
}

while :
do
    case "$1" in
      -d | --git-directory)
          src_dir="$2"
          shift 2
          ;;
      -h | --help)
          display_help 
          exit 0
          ;;
      -u | --unit-tests)
          build_unit_tests=1
          shift 1;
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

if [ ! -d "$src_dir" ]; then
    echo "Source directory does not exist: '$src_dir'"
    exit 1
fi

check_linux_v4_version "$src_dir"

build_dir="$src_dir/build-linux"

if [ -d "$build_dir" ]; then
    echo "Build directory already exists: '$build_dir'. Cleaning it."
    rm -rf "$build_dir"
fi

artifact_dir="$build_dir/install"
app_dir="$build_dir/AppDir"
build_type="RelWithDebInfo"

export PATH="$HOME/.local/bin:$PATH" # the conan executable is located in ~/.local/bin on the ci runner

conan_dependencies_folder="$build_dir/conan/dependencies"
source "$src_dir/infomaniak-build-tools/conan/common-utils.sh"

echo
echo "Build type: $build_type"
echo "Source directory '$src_dir'"
echo "Build directory '$build_dir'"
echo "AppDir '$app_dir'"
echo "Artifact directory '$artifact_dir'"
echo

build_release() {
  mkdir -p "$app_dir"
  mkdir -p "$build_dir"

  conan_folder="$build_dir/conan"
  bash "$src_dir/infomaniak-build-tools/conan/build_dependencies.sh" $build_type "--output-dir=$conan_folder" --make-release

  conan_toolchain_file="$(find "$conan_folder" -name 'conan_toolchain.cmake' -print -quit 2>/dev/null | head -n 1)"
  conan_generator_folder="$(dirname "$conan_toolchain_file")"

  if [ ! -f "$conan_toolchain_file" ]; then
    echo "Conan toolchain file not found: $conan_toolchain_file"
    exit 1
  fi

  QTDIR="$(find_qt_conan_path "$build_dir")"
  export QTDIR
  export QMAKE="$QTDIR/bin/qmake"
  export PATH="$QTDIR/bin:$QTDIR/libexec:$HOME/.local/bin:$PATH"
  export LD_LIBRARY_PATH="$QTDIR/lib:$LD_LIBRARY_PATH"
  export PKG_CONFIG_PATH="$QTDIR/lib/pkgconfig:$PKG_CONFIG_PATH"

  source "$conan_generator_folder/conanbuild.sh"

  mkdir -p "$build_dir/build"
  cd "$build_dir/build"

  export KDRIVE_DEBUG=0

  cmake -B"$build_dir/build" -H"$src_dir" \
      -DQT_FEATURE_neon=OFF \
      -DCMAKE_BUILD_TYPE=$build_type \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DBIN_INSTALL_DIR="$build_dir/build/bin" \
      -DBUILD_UNIT_TESTS="$build_unit_tests" \
      -DBUILD_GUI=ON \
      -DBUILD_GUI_LEGACY=OFF \
      -DKDRIVE_THEME_DIR="$src_dir/infomaniak" \
      -DKDRIVE_DEPLOY_QT_RUNTIME=ON \
      -DQT_ENABLE_VERBOSE_DEPLOYMENT=ON \
      -DCONAN_DEP_DIR="$conan_dependencies_folder" \
      -DCMAKE_TOOLCHAIN_FILE="$conan_toolchain_file" \

  make -j"$(nproc)"

  v4_extract_debug_symbols ./bin "$build_dir" kDrive kdrive_qml

  make DESTDIR="$app_dir" install

  cp "$src_dir/sync-exclude-linux.lst" "$build_dir/build/bin/sync-exclude.lst"
  cp "$src_dir/sync-folder-rules-linux.csv" "$build_dir/build/bin/sync-folder-rules.csv"
}

package_release() {
  QTDIR="$(find_qt_conan_path "$build_dir")"
  export QTDIR

  local full_version

  v4_prepare_appdir "$app_dir"
  v4_check_appdir "$app_dir"

  cd "$build_dir"
  v4_linuxdeploy_deploy "$app_dir"
  v4_strip_unneeded_symbols "$app_dir"
  v4_verify_bundle "$app_dir"
  v4_package_appimage "$app_dir"

  full_version="$(grep "KDRIVE_VERSION_FULL" "$build_dir/build/version.h" | awk '{print $3}')"
  mkdir -p "$artifact_dir"
  mv kDrive*.AppImage "$artifact_dir/kDrive-${full_version}-amd64.AppImage"
}

package_recovery_updater() {
  local updater_bin="$app_dir/usr/bin/kDriveRecoveryUpdater"
  local updater_appdir="$build_dir/updater-app"
  local full_version
  local updater_appimage

  if [ ! -f "$updater_bin" ]; then
    echo "kDriveRecoveryUpdater not found at '$updater_bin'; the release requires its AppImage." >&2
    return 1
  fi

  v4_prepare_recovery_updater_appdir \
    "$app_dir" \
    "$updater_appdir" \
    "$src_dir/infomaniak/theme/colored/512-kdrive-recovery-updater-icon.png"
  v4_check_recovery_updater_appdir "$updater_appdir"

  cd "$build_dir"
  v4_linuxdeploy_recovery_updater "$updater_appdir"
  v4_strip_unneeded_symbols "$updater_appdir"
  v4_verify_bundle "$updater_appdir"
  v4_package_appimage "$updater_appdir"

  full_version="$(grep "KDRIVE_VERSION_FULL" "$build_dir/build/version.h" | awk '{print $3}')"
  updater_appimage="kDriveRecoveryUpdater-${full_version}-amd64.AppImage"
  mkdir -p "$artifact_dir"
  mv kDriveRecoveryUpdater*.AppImage "$artifact_dir/$updater_appimage"

  echo "Recovery updater AppImage created: $artifact_dir/$updater_appimage"
}

echo "Building ..."
build_release

echo
echo "Packaging recovery updater ..."
package_recovery_updater

rm -f "$app_dir/usr/bin/kDriveRecoveryUpdater"
check_main_appdir_updater_separation "$app_dir"

echo
echo "Packaging ..."
package_release

echo

echo "Done."
