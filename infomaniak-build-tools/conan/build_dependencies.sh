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

# ----------------------------------------------------------------------
# This script is used to build the dependencies of the kDrive client.
# It will use conan to install the dependencies.

if [[ "${1:-}" =~ ^-h|--help$ ]]; then
  echo "Usage: $0 [Debug|Release] [--output-dir=<output_dir>]"
  exit 0
fi

set -euo pipefail

log(){ echo "[INFO] $*"; }
error(){ echo "[ERROR] $*" >&2; exit 1; }

PLATFORM=$(uname | tr '[:upper:]' '[:lower:]')
if [ "$PLATFORM" != "darwin" ] && [ "$PLATFORM" != "linux" ]; then
  error "Unsupported platform: $PLATFORM. Supported platforms: Linux, macOS"
fi

# check if we launched this in the right folder.
if [ ! -d "infomaniak-build-tools/conan" ]; then
  error "Please run this script from the root of the repository."
fi

if ! command -v conan >/dev/null 2>&1; then
  error "Conan is not installed. Please install it first."
fi

CONAN_REMOTE_BASE_FOLDER="$PWD/infomaniak-build-tools/conan/"
CONAN_RECIPES_FOLDER="$CONAN_REMOTE_BASE_FOLDER/recipes"

LOCAL_RECIPE_REMOTE_NAME="localrecipes"
if ! conan remote list | grep -qE "^$LOCAL_RECIPE_REMOTE_NAME.*\[.*Enabled: True.*\]"; then
  log "Adding local recipe remote."
  conan remote add "$LOCAL_RECIPE_REMOTE_NAME" "$CONAN_REMOTE_BASE_FOLDER"
else
  log "Local recipe remote already added."
fi

# Build conan recipe for the platforms x86_64 & arm64
MACOS_ARCH=""
if [ "$PLATFORM" = "darwin" ]; then
  log "Building universal binary for macOS."
  MACOS_ARCH="-s:a=arch=armv8|x86_64" # Making universal binary. See https://docs.conan.io/2/reference/tools/cmake/cmaketoolchain.html#conan-tools-cmaketoolchain-universal-binaries
fi

BUILD_TYPE="${1:-Debug}"
OUTPUT_DIR=""
for arg in "$@"; do
  if [[ "$arg" =~ ^--output-dir= ]]; then
    OUTPUT_DIR="${arg#--output-dir=}"
    break
  fi
done

if [ -z "$OUTPUT_DIR" ]; then
  if [ "$BUILD_TYPE" = "Debug" ]; then
    log "Building in Debug mode."
    OUTPUT_DIR="../CLion-build-debug/"
  else
    log "Building in Release mode."
    if [ "$PLATFORM" = "darwin" ]; then
      OUTPUT_DIR="./build-macos/client"
    else
      OUTPUT_DIR="./build-linux/build"
    fi
  fi
fi
mkdir -p "$OUTPUT_DIR"


# Create the conan package for xxHash
log "Creating package xxHash..."
conan create "$CONAN_RECIPES_FOLDER/xxhash/all/" --build=missing $MACOS_ARCH -s:a=build_type="$BUILD_TYPE" -r=$LOCAL_RECIPE_REMOTE_NAME


log "Installing dependencies..."
# Install this packet in the build folder.
conan install . --output-folder="$OUTPUT_DIR" --build=missing $MACOS_ARCH -s:a=build_type="$BUILD_TYPE" -r=$LOCAL_RECIPE_REMOTE_NAME

log "Conan dependencies installed successfully."
