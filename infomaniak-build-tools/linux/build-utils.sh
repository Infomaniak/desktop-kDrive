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

script_directory_path="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
source "$script_directory_path/../version-helpers.sh"

function get_host_arch() {
    case "$(uname -m)" in
    x86_64) architecture="amd64" ;;
    arm64)  architecture="arm64" ;;
    aarch64)  architecture="arm64" ;;
    *) echo "Unsupported architecture: $(uname -m)" >&2; exit 1 ;;
    esac

    echo $architecture
}

function get_default_src_dir() {
  if [[ -n "$KDRIVE_SRC_DIR" ]]; then
     echo "$KDRIVE_SRC_DIR"
  elif [[ -d "$HOME/Projects/desktop-kDrive" ]]; then
     echo "$HOME/Projects/desktop-kDrive"
  else
     echo "$PWD"
    fi
}

function get_linux_major_version() {
    local version
    version="$(GetVersionFromJson "$1" false "Linux")" || return 1
    echo "${version%%.*}"
}

function get_linux_release_flavor() {
    local major
    major="$(get_linux_major_version "$1")" || return 1

    if (( major >= 4 )); then
        echo "v4"
    elif (( major == 3 )); then
        echo "legacy"
    else
        echo "Unsupported Linux major version '$major' in '$1/version.json'" >&2
        return 1
    fi
}

function get_linux_client_executable() {
    local flavor
    flavor="$(get_linux_release_flavor "$1")" || return 1

    if [[ "$flavor" == "v4" ]]; then
        echo "kdrive_qml"
    else
        echo "kDrive_client"
    fi
}
