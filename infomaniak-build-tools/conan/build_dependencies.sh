#! /usr/bin/env bash
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


# Default values
build_type="Debug"
output_dir=""
use_release_profile=false
# Preserve original arguments for output_dir resolution
all_args=("$@")

log(){ echo "[INFO] $*"; }
error(){ echo "[ERROR] $*" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    Debug|Release|RelWithDebInfo)
      build_type="$1"
      shift
      ;;
    --make-release)
      use_release_profile=true
      shift
      ;;
    -h|--help)
      cat << EOF >&2
Usage: $0 [Debug|Release] [--output-dir=<output_dir>] [--make-release] [--help]
  --help               Display this help message.
  --output-dir=<dir>   Set the output directory for the Conan packages.
  --make-release       Use the 'infomaniak_release' Conan profile.

There are three ways to set the output directory (in descending order of priority):
    1. By passing the --output-dir=<dir> parameter.
    2. By setting the KDRIVE_OUTPUT_DIR environment variable.
    3. By default, the output directory is set to:
       - ./build-macos/client for macOS
       - ./build-linux/build for Linux
EOF
      exit 0
      ;;
    --output-dir=*) # Used by get_output_dir function
      shift
      ;;
    *)
      error "Unknown option: $1"
      ;;
  esac
done



set -euox pipefail

function get_platform {
  platform="$(uname | tr '[:upper:]' '[:lower:]')"
  echo "$platform"
}

function get_architecture {
  platform=$1
  architecture="" # Left empty for Linux systems.
  if [[ "$platform" = "darwin" ]]; then
    architecture="-s:a=arch=armv8|x86_64" # Making universal binary. See https://docs.conan.io/2/reference/tools/cmake/cmaketoolchain.html#conan-tools-cmaketoolchain-universal-binaries
  fi

  echo "$architecture"
}

function get_arg_value {
  local prefix="$1="
  for arg in "${all_args[@]}"; do
    if [[ $arg == $prefix* ]]; then
      echo "${arg#"$prefix"}"
      return 0
    fi
  done
  echo ""
}

function get_env_var_value {
  local var_name="$1"
  if [[ -n "${!var_name:-}" ]]; then
    echo "${!var_name}"
  else
    echo ""
  fi
}

function get_default_output_dir {
  if [[ "$(get_platform)" = "darwin" ]]; then
    echo "./build-macos/client"
  else
    echo "./build-linux/build"
  fi
}

output_dir="$(get_arg_value '--output-dir')"
if [[ -z "${output_dir}" ]]; then
  env_output_dir="$(get_env_var_value 'KDRIVE_OUTPUT_DIR')"
  if [[ -n "${env_output_dir}" ]]; then
    output_dir="${env_output_dir}"
    log "Using environment variable 'KDRIVE_OUTPUT_DIR' as conan output_dir : '${output_dir}'" >&2
  else
    log "No output directory specified. Using default output directory." >&2
    output_dir="$(get_default_output_dir)"
  fi
fi


# check if we launched this in the right folder.
if [ ! -d "infomaniak-build-tools/conan" ]; then
  error "Please run this script from the root of the repository."
fi

if ! command -v conan >/dev/null 2>&1; then
  log "PATH: $PATH"
  error "Conan is not installed. Please install it first."
fi

# Check if a conan profile exists
has_profile() {
  conan profile list 2>/dev/null | grep -v '\.cmake$' | grep -qx "$1"
}


if [[ $use_release_profile == true ]]; then
  release_profile="infomaniak_release"
  if has_profile "$release_profile"; then
    profile_path=$(conan profile path "$release_profile")
    if ! grep -qE 'build_type=(Release|RelWithDebInfo)' "$profile_path"; then
      error "Profile '$release_profile' must set build_type to Release or RelWithDebInfo"
    fi
    if grep -q 'tools.cmake.cmaketoolchain:user_toolchain' "$profile_path"; then
      error "Profile '$release_profile' must not set tools.cmake.cmaketoolchain:user_toolchain"
    fi
    if grep -q 'os=Macos' "$profile_path" && ! grep -q 'arch=armv8|x86_64' "$profile_path"; then
      error "Profile '$release_profile' must set arch=armv8|x86_64 for MacOS"
    fi

    log "Using '$release_profile' profile for Conan."
    conan_profile="$release_profile"
  else
    error "Profile '$release_profile' does not exist. Please create it."
  fi
else
  log "Using default 'default' profile for Conan."
  conan_profile="default"
fi


set -euox pipefail
conan_remote_base_folder="$PWD/infomaniak-build-tools/conan"
local_recipe_remote_name="localrecipes"
if ! conan remote list | grep -qE "^$local_recipe_remote_name.*\[.*Enabled: True.*\]"; then
    log "Adding Conan remote '$local_recipe_remote_name' at '$conan_remote_base_folder'. "
    conan remote add "$local_recipe_remote_name" "$conan_remote_base_folder"
else
    log "Conan remote '$local_recipe_remote_name' already exists and is enabled."
fi

# Build conan recipe for the platforms x86_64 & arm64
platform=$(get_platform)

if [ "$platform" != "darwin" ] && [ "$platform" != "linux" ]; then
    error "Unsupported platform: $platform. Supported platforms: Linux, macOS."
fi

if [[ "$platform" == "darwin" ]]; then
    log "Building universal binary for macOS."
fi

architecture=$(get_architecture "$platform")

mkdir -p "$output_dir"

echo
log "Configuration:"
log "--------------"
log "- Platform: '$platform'"
log "- Architecture option: '$architecture'"
log "- Build type: '$build_type'"
log "- Output directory: '$output_dir'"
echo



# Create the conan package for xxHash.
conan_recipes_folder="$conan_remote_base_folder/recipes"
log "Creating package xxHash..."
conan create "$conan_recipes_folder/xxhash/all/" --build=missing $architecture -s:a=build_type=Release --profile:all="$conan_profile" -r=$local_recipe_remote_name

if [ "$platform" = "darwin" ]; then
  log "Creating openssl package..."
  conan create "$conan_recipes_folder/openssl-universal/all/" --build=missing -s:a=build_type=Release --profile:all="$conan_profile" -r="$local_recipe_remote_name" -r=conancenter
fi

qt_version="6.2.3"
if [[ "$platform" == "linux" ]] && [[ "$(uname -m)" == "aarch64" ]]; then
  qt_version="6.7.3"
fi

log "Creating Qt package..."
conan create "$conan_recipes_folder/qt/all/" --version="$qt_version" --build=missing $architecture -s:a=build_type=Release -r=$local_recipe_remote_name -r=conancenter


log "Creating sentry package..."
conan create "$conan_recipes_folder/sentry/all/" --build=missing $architecture -s:a=build_type=Release -r=$local_recipe_remote_name -r=conancenter

log "Installing dependencies..."
# Install this packet in the build folder.
# Here: -s:b set the build type for the app itself, -s:h set the build type for the host (dependencies).
conan install . --output-folder="$output_dir" --build=missing $architecture -s:b=build_type="$build_type" -s:h=build_type="Release" --profile:all="$conan_profile" -r=$local_recipe_remote_name -r=conancenter

if [ $? -ne 0 ]; then
  error "Failed to install Conan dependencies."
fi
log "Conan dependencies installed successfully for platform $platform ($build_type) in '$output_dir'."
