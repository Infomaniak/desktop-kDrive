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

set -ex

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

podman machine stop build_kdrive
ulimit -n unlimited
podman machine start build_kdrive
podman run --rm -it \
	--privileged \
	--ulimit nofile=4000000:4000000 \
	--volume "$HOME/Projects/desktop-kDrive:/src" \
	--volume "$build_dir:/build" \
	--volume "$install_dir:/install" \
	--volume "$conan_cache_folder:/root/.conan2/p" \
	--volume "$local_recipes_index:/root/.conan2/.local_recipes_index/" \
	--workdir "/src" \
	--env APPLICATION_SERVER_URL="$APPLICATION_SERVER_URL" \
	--env KDRIVE_VERSION_BUILD="$(date +%Y%m%d)" \
	--arch amd64 \
	ghcr.io/infomaniak/kdrive-desktop-linux:amd64 /bin/bash -c "/src/infomaniak-build-tools/linux/build-release-appimage-amd64.sh"
podman machine stop build_kdrive

version=$(grep "KDRIVE_VERSION_FULL" "$build_dir/client/version.h" | awk '{print $3}')

mv "$install_dir/kDrive-x86_64.AppImage" "$install_dir/kDrive-$version-amd64.AppImage"

rm -Rf "$build_dir-amd64"
mv "$build_dir" "$build_dir-amd64"
