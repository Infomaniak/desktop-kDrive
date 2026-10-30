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
# With --only-damaged, a package folder is removed only when it carries the pruning damage
# signature: its CMake SQL plugin configurations (lib/cmake/Qt6Sql/Qt6Q*DriverPluginConfig.cmake)
# reference more SQL drivers than the sqldrivers folder actually contains. Package folders
# without the CMake SQL plugin configurations cannot be assessed and are kept. This makes
# the repair safe to run from the CI jobs themselves: it is a no-op on healthy caches.
#
# Equivalent Conan CLI alternative (removes all Qt revisions from one home):
#   CONAN_HOME=<home> conan remove "qt/*" -c
#
# Usage:
#   repair_conan_cache.sh [-h] [-y] [--only-damaged] [--home <conan-home>]...
#
# where:
#   -h, --help         Show this help text.
#   -y, --yes          Delete the matched package folders (default: dry-run).
#   --only-damaged     Only delete folders carrying the pruning damage signature.
#   --home <dir>       Conan home to repair. Repeatable. Defaults to '$HOME/.conan2'.
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
only_damaged=false

function display_help {
  echo "$program_name [-h] [-y] [--only-damaged] [--home <conan-home>]..."
  echo "  Remove the Qt package folders from one or more Conan cache homes so they are"
  echo "  re-created by the next 'conan install'. Dry-run unless --yes is given."
  echo "where:"
  echo "-h, --help        Show this help text."
  echo "-y, --yes         Delete the matched package folders (default: dry-run)."
  echo "--only-damaged    Only delete folders carrying the pruning damage signature."
  echo "--home <dir>      Conan home to repair. Repeatable. Defaults to '$HOME/.conan2'."
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
      --only-damaged)
          only_damaged=true
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

# Assess a Qt package folder for the pruning damage signature, using two independent
# signals:
#   1. its CMake SQL plugin configurations (lib/cmake/Qt6Sql/Qt6Q*DriverPluginConfig.cmake)
#      reference more SQL drivers than the sqldrivers folder actually contains;
#   2. the sqldrivers folder was reduced to the SQLite driver alone, which the official Qt
#      packages never ship (they always bundle several SQL drivers).
# Folders matching neither signal cannot be assessed and report 'unknown'.
function qt_package_damage_report {
    local package_root="$1" # the 'p' folder holding the package content
    local cmake_sql_dir="$package_root/lib/cmake/Qt6Sql"
    local sqldrivers_dir="$package_root/plugins/sqldrivers"
    local plugin_configs=()
    local driver_libs=()

    if [[ -d "$cmake_sql_dir" ]]; then
        plugin_configs=("$cmake_sql_dir"/Qt6Q*DriverPluginConfig.cmake)
    fi
    if [[ -d "$sqldrivers_dir" ]]; then
        driver_libs=("$sqldrivers_dir"/*.so)
    fi

    if [[ ${#plugin_configs[@]} -gt 0 && ${#driver_libs[@]} -lt ${#plugin_configs[@]} ]]; then
        echo "damaged (${#driver_libs[@]} driver libraries for ${#plugin_configs[@]} SQL plugin configurations)"
    elif [[ ${#driver_libs[@]} -eq 1 && -e "$sqldrivers_dir/libqsqlite.so" ]]; then
        echo "damaged (only the SQLite driver remains in sqldrivers)"
    elif [[ ${#plugin_configs[@]} -eq 0 ]]; then
        echo "unknown (no CMake SQL plugin configuration found)"
    else
        echo "healthy (${#driver_libs[@]} driver libraries for ${#plugin_configs[@]} SQL plugin configurations)"
    fi
}

function repair_conan_home {
    local home="$1"
    local candidate
    local deletable=()

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
        local report
        report="$(qt_package_damage_report "$candidate/p")"
        echo "  Matched Qt package folder: $candidate"
        if [[ -d "$candidate/p/plugins/sqldrivers" ]]; then
            echo "    sqldrivers content: $(ls "$candidate/p/plugins/sqldrivers" | tr '\n' ' ')"
        else
            echo "    sqldrivers folder missing: $candidate/p/plugins/sqldrivers"
        fi
        echo "    Assessment: $report"
        if [[ "$only_damaged" == true && "$report" != damaged* ]]; then
            echo "    Skipped (--only-damaged)."
            continue
        fi
        deletable+=("$candidate")
    done

    if [[ ${#deletable[@]} -eq 0 ]]; then
        echo "  No Qt package folder to remove."
        return 0
    fi

    if [[ "$apply_changes" == true ]]; then
        rm -rf "${deletable[@]}"
        echo "  Removed ${#deletable[@]} package folder(s)."
    else
        echo "  Dry-run: no folder removed. Re-run with --yes to delete them."
    fi
}

for home in "${conan_homes[@]}"; do
    repair_conan_home "$home"
done
