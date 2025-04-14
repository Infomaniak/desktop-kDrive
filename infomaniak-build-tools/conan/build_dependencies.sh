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

PLATFORM=$(uname)
if [ "$PLATFORM" != "Darwin" ] && [ "$PLATFORM" != "Linux" ]; then
    echo "Unsupported platform: $PLATFORM"
    echo "Supported platforms: Linux, macOS"
    exit 1
fi

# check if we launched this in the right folder.
if [ ! -d "infomaniak-build-tools/conan" ]; then
  echo "Please run this script from the root of the repository."
  exit 1
fi

CONAN_REMOTE_BASE_FOLDER="$PWD/infomaniak-build-tools/conan/"
CONAN_RECIPES_FOLDER="$CONAN_REMOTE_BASE_FOLDER/recipes"

LOCAL_RECIPE_REMOTE_NAME="localrecipes"
if ! conan remote list | grep -qE "^$LOCAL_RECIPE_REMOTE_NAME.*\[.*Enabled: True.*\]"; then
  echo "Adding local recipe remote."
  conan remote add "$LOCAL_RECIPE_REMOTE_NAME" "$CONAN_REMOTE_BASE_FOLDER"
else
  echo "Local recipe remote already added."
fi

# Build conan recipe for the platforms x86_64 & arm64
echo "Creating package xxHash"
MACOS_ARCH=""
if [ "$PLATFORM" = "Darwin" ]; then
#  echo "Building universal binary for macOS."
  MACOS_ARCH="-s:a=arch=armv8|x86_64"
fi

BUILD_TYPE="Debug"
OUTPUT_DIR=""
if [ "$BUILD_TYPE" = "Debug" ]; then
  echo "Building in Debug mode."
  OUTPUT_DIR="../CLion-build-debug/"
else
  echo "Building in Release mode."
  OUTPUT_DIR="./build-$PLATFORM/client" # TODO Fix $PLATFORM (Darwin => macOS)
fi


# Create the conan package for xxHash
conan create "$CONAN_RECIPES_FOLDER/xxhash/all/" --build=missing $MACOS_ARCH -s:a=build_type="$BUILD_TYPE" -r=$LOCAL_RECIPE_REMOTE_NAME

# Install this packet in the build folder.
conan install . --output-folder="$OUTPUT_DIR" --build=missing $MACOS_ARCH -s:a=build_type="$BUILD_TYPE" -r=$LOCAL_RECIPE_REMOTE_NAME
