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
  cat << EOF >&2
Usage: $0 [Debug|Release] [--output-dir=<output_dir>]
  There are three ways to set the output directory (in descending order of priority):
    1. --output-dir=<output_dir> argument
    2. KDRIVE_OUTPUT_DIR environment variable
    3. Default directory based on the system (macOS: build-macos/client, Linux: build-linux/build)
EOF
  exit 0
fi

set -euo pipefail

log(){ echo "[INFO] $*"; }
error(){ echo "[ERROR] $*" >&2; exit 1; }

platform=$(uname | tr '[:upper:]' '[:lower:]')
if [ "$platform" != "darwin" ] && [ "$platform" != "linux" ]; then
  error "Unsupported platform: $platform. Supported platforms: Linux, macOS"
fi

# check if we launched this in the right folder.
if [ ! -d "infomaniak-build-tools/conan" ]; then
  error "Please run this script from the root of the repository."
fi

if ! command -v conan >/dev/null 2>&1; then
  error "Conan is not installed. Please install it first."
fi

conan_remote_base_folder="$PWD/infomaniak-build-tools/conan"
conan_recipes_folder="$conan_remote_base_folder/recipes"

local_recipe_remote_name="localrecipes"
if ! conan remote list | grep -qE "^$local_recipe_remote_name.*\[.*Enabled: True.*\]"; then
  log "Adding Conan remote '$local_recipe_remote_name' at '$conan_remote_base_folder'. "
  conan remote add "$local_recipe_remote_name" "$conan_remote_base_folder"
else
  log "Conan remote '$local_recipe_remote_name' already exists and is enabled."
fi

# Build conan recipe for the platforms x86_64 & arm64
macos_arch=""
if [ "$platform" = "darwin" ]; then
  log "Building universal binary for macOS."
  macos_arch="-s:a=arch=armv8|x86_64" # Making universal binary. See https://docs.conan.io/2/reference/tools/cmake/cmaketoolchain.html#conan-tools-cmaketoolchain-universal-binaries
fi

build_type="${1:-Debug}"
output_dir="${KDRIVE_OUTPUT_DIR:-}"
if [ -n "${KDRIVE_OUTPUT_DIR:-}" ]; then
  log "Using environment variable 'KDRIVE_OUTPUT_DIR' as conan output_dir : '$KDRIVE_OUTPUT_DIR'"
fi
for arg in "$@"; do
  if [[ "$arg" =~ ^--output-dir= ]]; then
    output_dir="${arg#--output-dir=}"
    break
  fi
done

if [ -z "$output_dir" ]; then
  if [ "$platform" = "darwin" ]; then
    output_dir="./build-macos/client"
  else
    output_dir="./build-linux/build"
  fi
fi
mkdir -p "$output_dir"


# Create the conan package for xxHash
log "Creating package xxHash..."
conan create "$conan_recipes_folder/xxhash/all/" --build=missing $macos_arch -s:a=build_type="$build_type" -r=$local_recipe_remote_name


log "Installing dependencies..."
# Install this packet in the build folder.
conan install . --output-folder="$output_dir" --build=missing $macos_arch -s:a=build_type="$build_type" -r=$local_recipe_remote_name

log "Conan dependencies installed successfully."
