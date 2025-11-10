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

function display_help {
  echo "$program_name [-h] [-d git-directory]"
  echo "  Build the Linux release AppImage for desktop-kDrive."
  echo "where:"
  echo "-h  Show this help text."
  echo "-d  Set the git directory path of the project to build. Defaults to '$git_dir'."
}

while [[ $# -gt 0 ]];
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
      -*|--*)
          echo "Error: Unknown option: $1" >&2
          exit 1
          ;;
      *)  # No more options
          break
          ;;
    esac
done

echo "Starting build via podman ..."

bash "$script_directory_path/build-release-via-podman.sh" -d "$git_dir"
