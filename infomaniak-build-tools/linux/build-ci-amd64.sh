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

set -xe

program_name="$(basename "$0")"

function display_help {
  echo "$program_name [-h] [-t build-type] [-u] [--test-only] [--ccache] [--extract-debug]"
  echo "  Build the desktop-kDrive application for Linux Amd64 with the specified build type ."
  echo "where:"
  echo "-h  Show this help text."
  echo "-t <build-type>" 
  echo "  Set the type of the build. Defaults to 'debug'. The valid values are: 'debug', 'release', or 'relwithdebinfo'."
  echo "-u"
  echo "  Activate the build of unit tests. Without this flag, unit tests will not be built."
  echo "--test-only"
  echo "  Build only the unit test executables and their dependencies. Intended for local CI-like iterations."
  echo "--ccache"
  echo "  Use ccache as C/C++ compiler launcher when available. Disabled by default; measure hit rate before enabling in CI."
  echo "--extract-debug"
  echo "  Extract debug symbols from kDrive and kDrive_client. Disabled by default in this CI build script."
}


unit_tests=0
build_type="debug"
test_only=0
use_ccache=0
extract_debug_symbols=0

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
      --test-only)
          test_only=1
          unit_tests=1
          shift 1
          ;;
      --ccache)
          use_ccache=1
          shift 1
          ;;
      --extract-debug)
          extract_debug_symbols=1
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


if [[ "$build_type" == "release" ]]; then
    build_type="Release"
elif [[ "$build_type" == "relwithdebinfo" ]]; then
    build_type="RelWithDebInfo"
elif [[ "$build_type" == "debug" ]]; then
    build_type="Debug"
fi

echo "Build type: $build_type"
echo "Unit tests build flag: $unit_tests"
echo "Test-only build flag: $test_only"
echo "ccache build flag: $use_ccache"
echo "Extract debug symbols flag: $extract_debug_symbols"

export BASEPATH="$PWD"
export CONTENTDIR="$BASEPATH/build-linux"
export BUILD_DIR="$CONTENTDIR/build"
export APPDIR="$CONTENTDIR/app"
conan_build_folder="$BUILD_DIR/conan"
conan_dependencies_folder="$BUILD_DIR/conan/dependencies"

export PATH="$HOME/.local/bin:$PATH" # the conan executable is located in ~/.local/bin on the ci runner
bash "$BASEPATH/infomaniak-build-tools/conan/build_dependencies.sh" "$build_type" --output-dir="$conan_build_folder" --clean-cache
conan_toolchain_file="$(find "$conan_build_folder" -name 'conan_toolchain.cmake' -print -quit 2>/dev/null | head -n 1)"

if [ ! -f "$conan_toolchain_file" ]; then
  echo "Conan toolchain file not found: $conan_toolchain_file"
  exit 1
fi


source "$BASEPATH/infomaniak-build-tools/conan/common-utils.sh"
QTDIR="$(find_qt_conan_path "$conan_build_folder")"
export QTDIR


extract_debug () {
    objcopy --only-keep-debug "$1/$2" "$CONTENTDIR/$2-amd64.dbg"
    objcopy --strip-debug "$1/$2"
    objcopy "--add-gnu-debuglink=$CONTENTDIR/kDrive-amd64.dbg" "$1/$2"
}

mkdir -p "$APPDIR"
mkdir -p "$BUILD_DIR"

export QMAKE="$QTDIR/bin/qmake"
export PATH="$QTDIR/bin:$QTDIR/libexec:$PATH"
export LD_LIBRARY_PATH="$QTDIR/lib:$LD_LIBRARY_PATH"
export PKG_CONFIG_PATH="$QTDIR/lib/pkgconfig:$PKG_CONFIG_PATH"

# Set defaults
cmake_generator_args=()
if command -v ninja >/dev/null 2>&1; then
    cmake_generator_args=(-G Ninja)
fi

cmake_cache_args=()
if [ "$use_ccache" -eq 1 ] && command -v ccache >/dev/null 2>&1; then
    export CCACHE_DIR="${CCACHE_DIR:-$HOME/.cache/ccache-kdrive-linux}"
    export CCACHE_BASEDIR="$BASEPATH"
    mkdir -p "$CCACHE_DIR"
    ccache --show-stats || true
    cmake_cache_args=(
        -DCMAKE_C_COMPILER_LAUNCHER=ccache
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
    )
elif [ "$use_ccache" -eq 1 ]; then
    echo "ccache requested but not found; building without compiler launcher."
fi

cmake_ci_args=()
if [ "$test_only" -eq 1 ]; then
    cmake_ci_args=(
        -DBUILD_GUI=OFF
        -DBUILD_EXTENSIONS=OFF
        -DBUILD_EXTENSIONS_ICONS=OFF
        -DBUILD_DOCS=OFF
    )
fi

# Build client
cd "$BUILD_DIR"

source "$(dirname "$conan_toolchain_file")/conanrun.sh"

export KDRIVE_DEBUG=0

cmake -B"$BUILD_DIR" -H"$BASEPATH" \
    "${cmake_generator_args[@]}" \
    -DQT_FEATURE_neon=OFF \
    -DCMAKE_BUILD_TYPE="$build_type" \
    -DCMAKE_PREFIX_PATH="$BASEPATH" \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBIN_INSTALL_DIR="$BUILD_DIR/bin" \
    -DKDRIVE_THEME_DIR="$BASEPATH/infomaniak" \
    -DBUILD_UNIT_TESTS=$unit_tests \
    -DCONAN_DEP_DIR="$conan_dependencies_folder" \
    -DCMAKE_TOOLCHAIN_FILE="$conan_toolchain_file" \
    "${cmake_cache_args[@]}" \
    "${cmake_ci_args[@]}"

if [ "$test_only" -eq 1 ]; then
    cmake --build "$BUILD_DIR" --parallel "$(nproc)" --target \
        kDrive_test_common \
        kDrive_test_common_server \
        kDrive_test_server \
        kDrive_test_syncengine \
        kDrive_test_parms
else
    cmake --build "$BUILD_DIR" --parallel "$(nproc)"

    if [ "$extract_debug_symbols" -eq 1 ]; then
        if [ -f ./bin/kDrive ]; then
            extract_debug ./bin kDrive
        fi
        if [ -f ./bin/kDrive_client ]; then
            extract_debug ./bin kDrive_client
        fi
    fi

    DESTDIR="$APPDIR" cmake --install "$BUILD_DIR"
fi

if [ "$use_ccache" -eq 1 ] && command -v ccache >/dev/null 2>&1; then
    ccache --show-stats || true
fi

cp "$BASEPATH/sync-exclude-linux.lst" "$BUILD_DIR/bin/sync-exclude.lst"
