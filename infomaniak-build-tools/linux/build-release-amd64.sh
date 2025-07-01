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

src_dir="$(get_default_src_dir)"

function display_help {
  echo "$program_name [-h] [-d git-directory]"
  echo "  Build the Linux Amd64 release executables built inside <git-directory>/build-linux-amd64."
  echo "where:"
  echo "-h  Show this help text."
  echo "-d <git-directory>" 
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


build_dir="$src_dir/build-linux-amd64"
app_dir="$build_dir/install"
build_type="RelWithDebInfo"

conan_dependencies_folder="$build_dir/conan_dependencies"

echo
echo "Build type: $build_type"
echo "Source directory '$src_dir'"
echo "Build directory '$build_dir'"
echo "Install directory '$app_dir'"
echo

extract_debug () {
    objcopy --only-keep-debug "$1/$2" "$build_dir/$2.dbg"
    objcopy --strip-debug "$1/$2"
    objcopy --add-gnu-debuglink="$build_dir/kDrive.dbg" "$1/$2"
}

build_release() {
  export QT_BASE_DIR="$HOME/Qt/6.2.3"
  export QTDIR="$QT_BASE_DIR/gcc_64"
  export QMAKE="$QTDIR/bin/qmake"
  export PATH="$QTDIR/bin:$QTDIR/libexec:$PATH"
  export LD_LIBRARY_PATH="$QTDIR/lib:$LD_LIBRARY_PATH"
  export PKG_CONFIG_PATH="$QTDIR/lib/pkgconfig:$PKG_CONFIG_PATH"

  mkdir -p "$app_dir"
  mkdir -p "$build_dir"

  conan_folder="$build_dir/conan"
  bash "$src_dir/infomaniak-build-tools/conan/build_dependencies.sh" $build_type "--output-dir=$conan_folder"

  conan_toolchain_file="$(find "$conan_folder" -name 'conan_toolchain.cmake' -print -quit 2>/dev/null | head -n 1)"
  conan_generator_folder="$(dirname "$conan_toolchain_file")"

  if [ ! -f "$conan_toolchain_file" ]; then
    echo "Conan toolchain file not found: $conan_toolchain_file"
    exit 1
  fi



  source "$conan_generator_folder/conanbuild.sh"

  # Set defaults
  export SUFFIX=""

  # Build client
  mkdir -p "$build_dir/client"
  cd "$build_dir/client"


  export KDRIVE_DEBUG=0

  cmake -B"$build_dir/client" -H"$src_dir" \
      -DQT_FEATURE_neon=OFF \
      -DCMAKE_BUILD_TYPE=$build_type \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DBIN_INSTALL_DIR="$build_dir/client/bin" \
      -DKDRIVE_VERSION_SUFFIX="$SUFFIX" \
      -DKDRIVE_THEME_DIR="$src_dir/infomaniak" \
      -DKDRIVE_VERSION_BUILD="$(date +%Y%m%d)" \
      -DCONAN_DEP_DIR="$conan_dependencies_folder" \
      -DCMAKE_TOOLCHAIN_FILE="$conan_toolchain_file" \

  make -j"$(nproc)"

  extract_debug ./bin kDrive
  extract_debug ./bin kDrive_client

  make DESTDIR="$app_dir" install

  cp "$src_dir/sync-exclude-linux.lst" "$build_dir/client/bin/sync-exclude.lst"
} 

package_release() {
  export QT_BASE_DIR="$HOME/Qt/6.2.3/gcc_64"
  export QTDIR="$QT_BASE_DIR"
  export QMAKE="$QT_BASE_DIR/bin/qmake"
  export PATH="$QT_BASE_DIR/bin:$QT_BASE_DIR/libexec:$PATH"
  export LD_LIBRARY_PATH="$QT_BASE_DIR/lib:$app_dir/usr/lib:/usr/local/lib:/usr/local/lib64:$LD_LIBRARY_PATH"
  export PKG_CONFIG_PATH="$QT_BASE_DIR/lib/pkgconfig:$PKG_CONFIG_PATH"

  mkdir -p "$app_dir/usr/plugins"
  cd "$app_dir"

  cp -P -r "$QTDIR/plugins/"* "$app_dir/usr/plugins/"
  cp -P -r "$QTDIR/libexec" "$app_dir/usr"
  cp -P -r "$QTDIR/resources" "$app_dir/usr"
  cp -P -r "$QTDIR/translations" "$app_dir/usr"

  mv "$app_dir/usr/lib/x86_64-linux-gnu/"* "$app_dir/usr/lib/" || echo "The folder $app_dir/usr/lib/x86_64-linux-gnu/ might not exist." >&2
  cp -P "$conan_dependencies_folder/"* "$app_dir/usr/lib"

  cp -P -r /usr/lib/x86_64-linux-gnu/nss/ "$app_dir/usr/lib/"

  cp -P "$QTDIR/lib/libQt6WaylandClient.so"* "$app_dir/usr/lib"
  cp -P "$QTDIR/lib/libQt6WaylandEglClientHwIntegration.so"* "$app_dir/usr/lib"

  mkdir -p "$app_dir/usr/qml"

  rm -rf "$app_dir/usr/lib/x86_64-linux-gnu/"
  rm -rf "$app_dir/usr/include/"

  cp "$src_dir/sync-exclude-linux.lst" "$app_dir/usr/bin/sync-exclude.lst"
  cp "$app_dir/usr/share/icons/hicolor/512x512/apps/kdrive-win.png" "$app_dir"

  cp "$HOME/Qt/Tools/QtCreator/lib/Qt/lib/libQt6SerialPort.so.6" "$app_dir/usr/lib/"

  "$HOME/desktop-setup/linuxdeploy-x86_64.AppImage" --appdir "$app_dir" -e "$app_dir/usr/bin/kDrive" -i "$app_dir/kdrive-win.png" -d "$app_dir/usr/share/applications/kDrive_client.desktop" --plugin qt --output appimage -v0

  full_version="$(grep "KDRIVE_VERSION_FULL" "$build_dir/client/version.h" | awk '{print $3}')"
  app_name="kDrive-${full_version}-amd64.AppImage"
  mv kDrive*.AppImage "$app_dir/$app_name"
}

echo "Building ..."
build_release

echo

echo "Packaging ..."
package_release

echo

echo "Done."
