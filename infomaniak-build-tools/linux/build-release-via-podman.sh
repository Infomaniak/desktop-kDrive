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

script_directory_path="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
source "$script_directory_path/build-utils.sh"

git_dir="$(get_default_src_dir)"
program_name="$(basename "$0")"

function display_help {
  echo "$program_name [-h] [-d git-directory]"
  echo "  Build the Linux release AppImage for desktop-kDrive via Podman."
  echo "where:"
  echo "-h  Show this help text."
  echo "-d  Set the git directory path that will be mapped to the image '/src' folder. Defaults to '$git_dir'."
}

while :
do
    case "$1" in
      -h | --help)
          display_help
          exit 0
          ;;
      -d | --git-dir)
          git_dir="$2"
          shift 2
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

build_dir="$PWD/build-linux"
client_dir="$build_dir/client"
install_dir="$build_dir/install"

conan_base_folder="$HOME/.conan2_linux"
conan_cache_folder="$conan_base_folder/p"
local_recipes_index="$conan_base_folder/.local_recipes_index"

rm -Rf "$build_dir"
mkdir -p "$build_dir"
mkdir -p "$client_dir"
mkdir -p "$install_dir"
mkdir -p "$conan_cache_folder"
mkdir -p "$local_recipes_index"

if [ ! -d "$git_dir" ]; then
    echo "Git directory does not exist: '$git_dir'"
    exit 1
fi

echo "Using git directory '${git_dir}'."
echo "Using temporary build folder '${build_dir}'."
architecture=$(get_host_arch)
echo "Building desktop-kDrive AppImage for architecture ${architecture} via Podman ..."
echo

set -ex

podman machine stop build_kdrive 2>/dev/null || true
podman wait build_kdrive

inode_max_limit=100000
ulimit_error=$( { ulimit -n $inode_max_limit; } 2>&1)
if [[ -n "$ulimit_error" ]]; then
    echo "Failed to set the max limit of open inodes with '$inode_max_limit'."
    echo "Current limit: '$(ulimit -n)'."
fi

podman machine start build_kdrive
podman run --rm -it \
	--privileged \
	--ulimit nofile=$inode_max_limit:$inode_max_limit \
	--volume "$git_dir:/src" \
	--volume "$build_dir:/build" \
	--volume "$install_dir:/install" \
	--volume "$conan_cache_folder:/root/.conan2/p" \
	--volume "$local_recipes_index:/root/.conan2/.local_recipes_index/" \
	--workdir "/src" \
	--env APPLICATION_SERVER_URL="$APPLICATION_SERVER_URL" \
	--env KDRIVE_VERSION_BUILD="$(date +%Y%m%d)" \
	--arch ${architecture} \
	ghcr.io/infomaniak/kdrive-desktop-linux:${architecture} /bin/bash -c "/src/infomaniak-build-tools/linux/build-release-appimage.sh"
podman machine stop build_kdrive

version=$(grep "KDRIVE_VERSION_FULL" "$build_dir/client/version.h" | awk '{print $3}')
mv "$install_dir/kDrive-${architecture}.AppImage" "$install_dir/kDrive-$version-${architecture}.AppImage"

rm -Rf "$build_dir-${architecture}"
mv "$build_dir" "$build_dir-${architecture}"

set -

echo
echo "Successfully built '${build_dir}-${architecture}/install/kDrive-${version}-${architecture}.AppImage'."
