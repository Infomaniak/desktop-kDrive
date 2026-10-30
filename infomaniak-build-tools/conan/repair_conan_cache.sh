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

# Repair a shared Conan cache by removing the Qt package folders from a Conan home, so
# that the next 'conan install' re-creates them from the recipes. Intended as a one-off
# repair after a Qt package folder was damaged in place (e.g. SQL drivers pruned from the
# cached package by older packaging scripts), a state Conan never re-validates on reuse.
#
# Only folders matching 'qt*' inside <home>/p/b/ (packages built locally) and <home>/p/
# (packages downloaded from remotes) are removed; every other package is left untouched.
# The script is a dry-run unless --yes is given.
#
# Equivalent Conan CLI alternative (removes all Qt revisions from one home):
#   CONAN_HOME=<home> conan remove "qt/*" -c
#
# Usage:
#   repair_conan_cache.sh [-h] [-y] [--home <conan-home>]...
#
# where:
#   -h, --help        Show this help text.
#   -y, --yes         Delete the matched package folders (default: dry-run).
#   --home <dir>      Conan home to repair. Repeatable. Defaults to '$HOME/.conan2'.
#
# Examples:
#   # Linux AMD64 runner (repair the shared runner cache):
#   repair_conan_cache.sh --home "$HOME/.conan2" --yes
#   # macOS ARM64 runner (host folder mounted into the Podman container as /root/.conan2/p):
#   repair_conan_cache.sh --home "$HOME/.conan2_linux" --yes

set -e

program_name="$(basename "$0")"
conan_homes=()
apply_changes=false

function display_help {
  echo "$program_name [-h] [-y] [--home <conan-home>]..."
  echo "  Remove the Qt package folders from one or more Conan cache homes so they are"
  echo "  re-created by the next 'conan install'. Dry-run unless --yes is given."
  echo "where:"
  echo "-h, --help     Show this help text."
  echo "-y, --yes      Delete the matched package folders (default: dry-run)."
  echo "--home <dir>   Conan home to repair. Repeatable. Defaults to '$HOME/.conan2'."
}

while :;
do
    case "$1" in
      -h | --help)
          display_help
          exit 0
          ;;
      -y | --yes)
          apply_changes=true
          shift
          ;;
      --home)
          conan_homes+=("$2")
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

if [[ ${#conan_homes[@]} -eq 0 ]]; then
    conan_homes=("$HOME/.conan2")
fi

# Package folders are stored as <home>/p/b/<5-char package name prefix><hash>/ for packages
# built locally and <home>/p/<5-char prefix><hash>/ for packages downloaded from remotes.
# With the Qt recipe being the only dependency whose name starts with 'qt', the 'qt*' glob
# matches exactly the Qt package folders.
shopt -s nullglob

function repair_conan_home {
    local home="$1"
    local candidate
    local sqldrivers_dir

    echo
    echo "Repairing Conan home '$home':"

    if [[ ! -d "$home/p" ]]; then
        echo "  No package store at '$home/p' - nothing to do."
        return 0
    fi

    local candidates=("$home/p/b/"qt* "$home/p/"qt*)
    if [[ ${#candidates[@]} -eq 0 ]]; then
        echo "  No Qt package folder found - nothing to do."
        return 0
    fi

    for candidate in "${candidates[@]}"; do
        echo "  Matched Qt package folder: $candidate"
        sqldrivers_dir="$candidate/p/plugins/sqldrivers"
        if [[ -d "$sqldrivers_dir" ]]; then
            echo "    sqldrivers content: $(ls "$sqldrivers_dir" | tr '\n' ' ')"
        else
            echo "    sqldrivers folder missing: $sqldrivers_dir"
        fi
    done

    if [[ "$apply_changes" == true ]]; then
        rm -rf "${candidates[@]}"
        echo "  Removed ${#candidates[@]} package folder(s)."
    else
        echo "  Dry-run: no folder removed. Re-run with --yes to delete them."
    fi
}

for home in "${conan_homes[@]}"; do
    repair_conan_home "$home"
done
